#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <wchar.h>
#include <thread>
#include <chrono>
#include <ctime>
#include <ctype.h>
#include <vector>
#include <msxml6.h>
#include <comdef.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <fcntl.h>
#include "types.h"
#include "sqll.h"

#pragma comment(lib, "msxml6.lib")     // xml parser
#pragma comment(lib, "winsqlite3.lib") // sqlite v3 for win
#pragma comment(lib, "winhttp.lib")    // http win client
#pragma comment(lib, "ole32.lib")      // CoInitialize and CoUninitialize

inline const std::wregex CHANNEL_ID_REGEX(L"^UC[A-Za-z0-9_-]{22}$");
constexpr wchar_t YOUTUBE_HOST[] = L"www.youtube.com";
constexpr uint32_t DEFAULT_LIMIT_CONSOLE = 20;
constexpr uint32_t DEFAULT_LIMIT_FEED = 30;
uint32_t limit = DEFAULT_LIMIT_CONSOLE;
uint32_t keep = DEFAULT_LIMIT_FEED;
char *filename = nullptr;
int newchan = 0;
std::vector<std::wstring> channels;
std::vector<Channel> chns;
HRESULT hr = S_OK;
bool single = false;
bool web = false;
bool news = false;
bool quiet = false;
Sqlite db(L"local.db");
std::vector<Video> videos;

size_t cv = 0;

bool parse_int(const char *str, int &out)
{
    if (str == nullptr || *str == '\0')
        return false;

    char *end = nullptr;
    errno = 0;

    long val = strtol(str, &end, 10);

    if (end == str) // no number
        return false;

    if (*end != '\0') // extra char
        return false;

    if (errno == ERANGE) // overflow/underflow
        return false;

    if (val < INT_MIN || val > INT_MAX)
        return false;

    out = static_cast<int>(val);
    return true;
}

bool isValidFilename(const char *filename)
{
    if (!filename)
        return false;

    int len = strlen(filename);
    if (len == 0)
        return false;

    // refuse "." ".."
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
        return false;

    // Windows not valid characters
    const char *invalid = "<>:\"/\\|?*";

    for (int i = 0; i < len; i++)
    {
        unsigned char c = filename[i];
        if (strchr(invalid, c))
            return false;
    }

    // extract only name file without path
    const char *base = strrchr(filename, '\\');
    if (!base)
        base = strrchr(filename, '/');
    if (!base)
        base = filename;
    else
        base++;

    // Reserved Windows name
    const char *reserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    for (int i = 0; i < 22; i++)
    {
        if (_stricmp(base, reserved[i]) == 0)
            return false;
    }

    // check extension
    const char *last_dot = strrchr(base, '.');
    if (!last_dot || last_dot == base)
        return false;

    const char *ext = last_dot + 1;
    int ext_len = strlen(ext);
    if (ext_len < 1 || ext_len > 4)
        return false;

    for (int i = 0; i < ext_len; i++)
    {
        if (!isalnum((unsigned char)ext[i]))
            return false;
    }

    return true;
}

std::string to_utf8(const std::wstring &w)
{
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(w);
}

std::wstring to_utf16(const std::string &s)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(s);
}

inline std::string u8s(const char *s)
{
    return std::string(s);
}

void readFile()
{
    channels.clear();
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::wcerr << L"Error: impossibile open file \n";
        return;
    }
    std::string line;

    // Regex ID channel YouTube
    // std::wregex re(L"^UC[A-Za-z0-9_-]{22}$");
    while (std::getline(file, line))
    {
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
        {
            line = line.substr(3);
        }
        // Trim
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        if (line.empty())
            continue;

        // conversion and check regex
        std::wstring wline = to_utf16(line);

        if (std::regex_match(wline, CHANNEL_ID_REGEX))
            channels.push_back(std::move(wline));
        else
            std::wcerr << L"Error: not valid id " << wline << L"\n";
    }
}

void parseXML(const wchar_t *wbuffer, Channel &channel)
{
    IXMLDOMDocument2 *pXMLDoc = nullptr;
    hr = CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pXMLDoc));

    if (FAILED(hr) || !pXMLDoc)
    {
        std::wcerr << L"Error: impossible create XML istance\n";
        exit(1);
    }

    VARIANT_BOOL loaded = VARIANT_FALSE;
    hr = pXMLDoc->loadXML(_bstr_t(wbuffer), &loaded);

    if (FAILED(hr) || loaded != VARIANT_TRUE)
    {
        std::wcerr << L"Error: parsing XML internal error\n";
        pXMLDoc->Release();
        exit(1);
    }

    // --- extract name channel ---
    IXMLDOMNode *pName = nullptr;
    pXMLDoc->selectSingleNode(_bstr_t(L"//*[local-name()='author']/*[local-name()='name']"), &pName);

    if (pName)
    {
        BSTR text;
        pName->get_text(&text);

        if (channel.name.empty())
        {
            channel.name = std::wstring(text);
            db.updateChannel(channel);
        }

        SysFreeString(text);
        pName->Release();
    }

    // --- extract entries ---
    IXMLDOMNodeList *pEntries = nullptr;
    pXMLDoc->selectNodes(_bstr_t(L"//*[local-name()='entry']"), &pEntries);

    if (pEntries)
    {
        long count = 0;
        pEntries->get_length(&count);

        for (long i = 0; i < count; ++i)
        {
            IXMLDOMNode *pEntry = nullptr;
            pEntries->get_item(i, &pEntry);

            if (!pEntry)
                continue;

            std::wstring title, id, times, link;
            bool is_short;

            auto extract = [&](std::wstring &key, const wchar_t *xpath)
            {
                IXMLDOMNode *pNode = nullptr;
                pEntry->selectSingleNode(_bstr_t(xpath), &pNode);
                if (pNode)
                {
                    BSTR text;
                    pNode->get_text(&text);
                    key = std::wstring(text);
                    SysFreeString(text);
                    pNode->Release();
                }
            };

            extract(title, L".//*[local-name()='title']");
            extract(id, L".//*[local-name()='videoId']");
            extract(times, L".//*[local-name()='published']");
            extract(link, L".//*[local-name()='link']/@href");

            is_short = (link.length() == 43) ? false : true;

            Video video(times, 0, channel.name, title, id, is_short);
            videos.emplace_back(std::move(video));

            pEntry->Release();
        }

        pEntries->Release();
    }

    pXMLDoc->Release();
}

void getFeed(HINTERNET &session, HINTERNET &connect, wchar_t *url, Channel &channel)
{

    HINTERNET hRequest = WinHttpOpenRequest(connect, L"GET", url,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    BOOL bResult = WinHttpSendRequest(hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0,
                                      0, 0);

    if (bResult)
        bResult = WinHttpReceiveResponse(hRequest, NULL);

    if (bResult)
    {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        std::wstring xmlBuffer;

        do
        {
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize == 0)
                break;

            BYTE *temp = new BYTE[dwSize + 1];
            ZeroMemory(temp, dwSize + 1);

            WinHttpReadData(hRequest, temp, dwSize, &dwDownloaded);

            int len = MultiByteToWideChar(CP_UTF8, 0, (char *)temp, dwDownloaded, NULL, 0);
            wchar_t *wtemp = new wchar_t[len + 1];
            MultiByteToWideChar(CP_UTF8, 0, (char *)temp, dwDownloaded, wtemp, len);
            wtemp[len] = L'\0';

            xmlBuffer += wtemp;

            delete[] temp;
            delete[] wtemp;
        } while (dwSize > 0);

        parseXML(xmlBuffer.c_str(), channel);
    }

    else
    {
        std::wcerr << L"Error: http request for url " << url << std::endl;
    }

    WinHttpCloseHandle(hRequest);
};

void generateHTML(const std::vector<Video> &videos)
{
    std::ofstream htmlFile("feed.html", std::ios::binary);
    if (!htmlFile.is_open())
        return;

    // Header
    htmlFile << u8s("<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'>"
                    "<title>YouTube Feed</title>"
                    "<style>"
                    "body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #0f0f0f; color: white; padding: 40px; }"
                    "h1, h3 { text-align: center; color: #ff0000; }"
                    ".stats { text-align: center; margin-bottom: 30px; color: #aaa; }"
                    ".grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 25px; max-width: 1400px; margin: 0 auto; }"
                    ".card { background: #1e1e1e; border-radius: 12px; overflow: hidden; transition: transform 0.2s; border: 1px solid #333; }"
                    ".card:hover { transform: scale(1.05); border-color: #ff0000; }"
                    "img { width: 100%; aspect-ratio: 16/9; object-fit: cover; }"
                    ".info { padding: 15px; }"
                    "a { text-decoration: none; color: white; }"
                    "h4 { font-size: 14px; margin: 0 0 8px 0; line-height: 1.4; height: 2.8em; overflow: hidden; }"
                    ".author { font-size: 12px; color: #aaa; margin: 0; }"
                    "</style></head><body>");

    htmlFile << "<h1>YouTube Feed Update</h1>";
    htmlFile << "<div class='stats'>New Videos: "
             << cv << " | New Channel: " << newchan << "</div>";

    htmlFile << "<div class='grid'>";

    // Loop video
    for (int i = 0; i < videos.size() && i < limit; i++)
    {
        htmlFile << "<div class='card'>"
                 << "<a href='https://www.youtube.com/watch?v=" << to_utf8(videos[i].id) << "' target='_blank'>"
                 << "<img src='https://i.ytimg.com/vi/" << to_utf8(videos[i].id) << "/mqdefault.jpg'>"
                 << "<div class='info'>"
                 << "<h4>" << to_utf8(videos[i].title) << "</h4>"
                 << "<p class='author'>" << to_utf8(videos[i].author) << "</p>"
                 << "</div></a></div>";
    }

    htmlFile << "</div></body></html>";
    htmlFile.close();

    system("start feed.html");
}

void printHelp()
{
    std::wcout << L"ytfeed-cli  available commands:\n\n"
                  "  -A, --add <id>         Add a single YouTube channel ID\n"
                  "  -C, --channel          Show all channels (ID + name) stored in the database\n"
                  "  -D, --delete <name>    Delete a channel by name\n"
                  "  -F, --feed <name>      Show feeds from a single channel by name\n"
                  "  -K, --keep <number>    Max videos saved per author in database from most recent (min: 30)\n"
                  "  -L, --load <file>      Load a list of YouTube channel IDs from a text file\n"
                  "  -N, --new              Show feeds published in last 24 hours\n"
                  "  -Q, --quiet            Run without network access, list only videos in database\n"
                  "  -S, --show <number>    Limit the number of feeds printed (default: 20)\n"
                  "  -X, --stat             Show feeds and database statistics\n"
                  "  -W, --web              Generate a static html page with feeds selected\n"
                  "  -H, --help             Show this help message\n\n";
}

int main(int argc, char *argv[])
{

    db.createTable(CT_CHANNELS);
    db.createTable(CT_VIDEOS);
    db.createTable(CT_SETTING);
    _setmode(_fileno(stdout), _O_U16TEXT);
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    for (int i = 1; i < argc; i++)
    {
        // --- LIMIT VIDEO SHOW IN CONSOLE -s ---
        if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--show") == 0)
        {
            int tmp;
            if (parse_int(argv[i + 1], tmp))
            {
                limit = tmp;
                i++;
            }
            continue;
        }
        // --- LIMIT VIDEO SAVED IN DB ---
        if (strcmp(argv[i], "-K") == 0 || strcmp(argv[i], "--keep") == 0)
        {
            if (i + 1 < argc)
            {
                int tmp;
                if (parse_int(argv[i + 1], tmp))
                {
                    keep = tmp < DEFAULT_LIMIT_FEED ? DEFAULT_LIMIT_FEED : tmp;
                    db.saveSettings(keep);
                    i++;
                }
            }
            continue;
        }
        // --- LOAD FILE -L ---
        if (strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--load") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing file name\n";
                exit(1);
            }

            filename = argv[i + 1];

            if (!isValidFilename(filename))
            {
                std::wcerr << L"Error: file name not valid\n";
                exit(1);
            }

            readFile();

            if (!channels.empty())
            {
                db.beginTransaction();
                for (const auto &c : channels)
                    newchan += db.insertChannel(c);
                db.commitTransaction();
               if(newchan > 0) db.saveSettings(-1);
            }

            i++;
            continue;
        }

        // --- ADD CHANNEL -A ---
        if (strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--add") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing channel Id\n";
                exit(1);
            }

            std::wstring wid = to_utf16(argv[i + 1]);

            if (!std::regex_match(wid, CHANNEL_ID_REGEX))
            {
                std::wcerr << L"Error: channel Id not valid " << wid << L"\n";
                exit(1);
            }

            newchan = db.insertChannel(wid);
            if(newchan > 0) db.saveSettings(-1);
            i++;
            continue;
        }

        // --- DELETE CHANNEL -D ---
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--delete") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing channel name\n";
                exit(1);
            }

            std::wstring wid = to_utf16(argv[i + 1]);
            int del = db.removeChannel(wid);

            if (del == 1)
                std::wcout << L"Channel deleted: " << wid << L"\n";
            else
                std::wcout << L"Channel not present: " << wid << L"\n";

            exit(0);
        }

        // --- SHOW ALL CHANNELS -C ---
        if (strcmp(argv[i], "-C") == 0 || strcmp(argv[i], "--channels") == 0)
        {
            db.extractChannels(chns);
            for (const auto &c : chns)
                c.printChannel();
            exit(0);
        }

        // --- SHOW SINGLE CHANNEL -F ---
        if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "--feed") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing channel name\n";
                exit(1);
            }

            std::wstring wid = to_utf16(argv[i + 1]);

            int c = db.extractSingleChannel(chns, wid);

            if (c == 0 && !chns.empty())
                single = true;
            else
            {
                std::wcout << L"Channel not present: " << wid << L"\n";
                exit(0);
            }
        }

        // --- NEW VIDEOS FLAG -n / --new ---
        if (strcmp(argv[i], "-N") == 0 || strcmp(argv[i], "--new") == 0)
        {
            news = true;
            continue;
        }
        // ---QUIET FLAG -n / --new ---
        if (strcmp(argv[i], "-Q") == 0 || strcmp(argv[i], "--quiet") == 0)
        {
            quiet = true;
            continue;
        }
        // --- WEB MODE -W ---
        if (strcmp(argv[i], "-W") == 0 || strcmp(argv[i], "--web") == 0)
        {
            web = true;
            continue;
        }

        // --- STATISTICS ---
        if (strcmp(argv[i], "--stat") == 0 || strcmp(argv[i], "-X") == 0)
        {
            db.stat();
            exit(0);
        }

        // --- HELP -H ---
        if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printHelp();
            exit(0);
        }
    }

    // flag only 1 channel feed
    if (!single)
        db.extractChannels(chns);

    videos.reserve(channels.size() * 15);

    // flag quiet/http request
    if (!quiet)
    {
        hr = CoInitialize(NULL);
        if (FAILED(hr))
        {
            std::wcerr << L"Error: CoInitialize failed\n";
            return 1;
        }
        HINTERNET hSession = WinHttpOpen(L"WinHTTP User Agent/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);

        HINTERNET hConnect = WinHttpConnect(hSession, YOUTUBE_HOST,
                                            INTERNET_DEFAULT_HTTPS_PORT, 0);

        for (auto &ch : chns)
        {
            wchar_t completeUrl[512];
            swprintf(completeUrl, 512, L"/feeds/videos.xml?channel_id=%s", ch.id.c_str());
            getFeed(hSession, hConnect, completeUrl, ch);

            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }

        cv = db.insertVideosBatch(videos);
  
        videos.clear();

        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        CoUninitialize(); 
    }

    for (const auto &ch : chns)
    {
        std::vector<Video> chanVideos;
        db.loadVideosAndTrim(chanVideos, ch.name);
        videos.insert(videos.end(), chanVideos.begin(), chanVideos.end());
    }

    // flag recent videos
    if (news)
        db.extractVideosLast24h(videos);

    std::sort(videos.begin(), videos.end(), [](const auto &a, const auto &b)
              { return a > b; });

    // flag web page
    if (web)
    {
        generateHTML(videos);
        exit(0);
    }

    std::wcout << L"\n----------------------------------------------------------------- YOUTUBE FEED UPDATE ---------------------------------------------------------------------\n\n";
    std::wcout << L"New Video(s): " << cv << "\n";
    if (newchan > 0)
        std::wcout << L"New Channel(s): " << newchan << "\n";
    for (int i = 0; i < videos.size(); i++)
    {
        if (i >= limit)
            break;
        videos[i].printVideo();
    }
    std::wcout << "\n";

    _setmode(_fileno(stdout), _O_TEXT);

    return 0;
}