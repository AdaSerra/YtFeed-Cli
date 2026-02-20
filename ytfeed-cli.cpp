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
#include <codecvt>
#include <fcntl.h>
#include "types.h"
#include "sqll.h"

#pragma comment(lib, "msxml6.lib") //xml parser
#pragma comment(lib, "winsqlite3.lib") //sqlite v3 for win
#pragma comment(lib, "winhttp.lib")    //http win client
#pragma comment(lib, "ole32.lib")  // CoInitialize and CoUninitialize

inline const std::wregex CHANNEL_ID_REGEX(L"^UC[A-Za-z0-9_-]{22}$");
constexpr wchar_t YOUTUBE_HOST[] = L"www.youtube.com";
constexpr uint32_t DEFAULT_LIMIT = 20;
uint32_t limit = DEFAULT_LIMIT;
char *filename = nullptr;
int newchan = 0;
std::vector<std::wstring> channels;
std::vector<Channel> chns;
HRESULT hr = S_OK;
bool single = false;
std::vector<Video> videos;
Sqlite db(L"local.db");
size_t cv = 0;

int is_number(const char *str)
{
    if (*str == '\0')
        return 0;

    if (*str == '-' || *str == '+')
    {
        str++;
        if (*str == '\0')
            return 0;
    }

    while (*str)
    {
        if (!isdigit(*str))
            return 0;
        str++;
    }
    return 1;
}

bool isValidFilename(const char *filename)
{
    if (!filename)
        return false;

    int len = strlen(filename);
    if (len == 0)
        return false;

    // Rifiuta "." e ".."
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

    // Reserved name Windows
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
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    // Regex ID channel YouTube
    // std::wregex re(L"^UC[A-Za-z0-9_-]{22}$");
    while (std::getline(file, line))
    { // Rimuovi BOM UTF-8 se presente
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
        std::wstring wline = conv.from_bytes(line);
        if (std::regex_match(wline, CHANNEL_ID_REGEX))
        {
            channels.push_back(std::move(wline));
        }
        else
        {
            std::wcerr << L"Error: not valid id " << wline << L"\n";
        }
    }
}

void parseXML(const wchar_t *wbuffer, Channel &channel)
{
    IXMLDOMDocument2 *pXMLDoc = nullptr;
    hr = CoCreateInstance(__uuidof(DOMDocument60), NULL, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pXMLDoc));

    if (FAILED(hr) || !pXMLDoc)
    {
        std::wcerr << L"Error: impossible create XML istance\n";
        return;
    }

    VARIANT_BOOL loaded = VARIANT_FALSE;
    hr = pXMLDoc->loadXML(_bstr_t(wbuffer), &loaded);

    if (FAILED(hr) || loaded != VARIANT_TRUE)
    {
        std::wcerr << L"Error: parsing XML internal error\n";
        pXMLDoc->Release();
        return;
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

            std::wstring title, id, times;

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

            Video video(times, 0, channel.name, title, id);
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

int main(int argc, char *argv[])
{

    //_setmode(_fileno(stdout), _O_U16TEXT);
    db.createTable(CT_CHANNELS);
    db.createTable(CT_VIDEOS);
    _setmode(_fileno(stdout), _O_U16TEXT);

    for (int i = 1; i < argc; i++)
    {
        // --- OPZIONE -s LIMIT ---
        if (_stricmp(argv[i], "-s") == 0)
        {
            if (i + 1 < argc && is_number(argv[i + 1]))
            {
                limit = atoi(argv[i + 1]);
                i++;
            }

            continue;
        }

        // --- OPTION -L FILENAME ---
        if (_stricmp(argv[i], "-L") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing file name \n";
                exit(1);
            }

            filename = argv[i + 1];

            if (!isValidFilename(filename))
            {
                std::wcerr << L"Error: file name not valid\n";
                exit(1);
            }
            else
            {

                readFile();

                if (!channels.empty())
                {
                    db.beginTransaction();
                    for (const auto &c : channels)
                    {
                        newchan += db.insertChannel(c);
                    }
                    db.commitTransaction();
                }
            }

            i++;
            continue;
        }

        // --- OPTION -A ---
        if (_stricmp(argv[i], "-A") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing channel Id\n";
                exit(1);
            } 

            std::string id_utf8 = argv[i + 1];
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring wid = conv.from_bytes(id_utf8);
            //  regex
            if (!std::regex_match(wid, CHANNEL_ID_REGEX))
            {
                std::wcerr << L"Error: channel Id not valid " << wid << L"\n";
                exit(1);
            } // insert DB
            newchan = db.insertChannel(wid);
           
            i++;
            continue;
        }

        // --- OPTION -D ---
        if (_stricmp(argv[i], "-D") == 0)
        {
            if (i + 1 >= argc)
            {
                std::wcerr << L"Error: missing channel name \n";
                exit(1);
            }

            std::string id_utf8 = argv[i + 1];
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring wid = conv.from_bytes(id_utf8);
            int del = db.removeChannel(wid);
            if (del == 1)
                std::wcout << L"Channel delete: " << wid << L"\n";
            else
                std::wcout << L"Channel not present: " << wid << L"\n";
            i++;
            continue;
        }

        if (_stricmp(argv[i], "-C") == 0)
        {
            db.extractChannels(chns);
            for (const auto &c : chns)
                c.printChannel();
            exit(0);
        }

        if(_stricmp(argv[i], "-F") == 0)
        {   
            std::string id_utf8 = argv[i + 1];
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring wid = conv.from_bytes(id_utf8);
            int c = db.extractSingleChannel(chns, wid);
            if (c == 0 && !chns.empty()) 
                single = true;
            else 
            {
                 std::wcout << L"Channel not present: " << wid << L"\n";
                 exit(0);
            }
        }
        if(_stricmp(argv[i], "-H") == 0)
        {
            std::wcout <<L"ytfeed-cli  available commands:\n\n"
                        "  -a, -A <id>       Add a single YouTube channel ID\n"
                        "  -c, -C            Show all channel (ID + name) stored in the database\n"
                        "  -d, -D <name>     Delete a channel by name\n"
                        "  -f, -F <name>     Show feeds from a single channel by name\n"
                        "  -l, -L <file>     Load a list of YouTube channel IDs from a text file\n"
                        "  -s, -S <number>   Limit the number of feeds printed (default: 20)\n"
                        "  -h, -H            Show this help message\n\n";
            

            exit(0);
        }
    }

    hr = CoInitialize(NULL);
    if (FAILED(hr))

    {
        std::wcerr << L"Error: CoInitialize failed \n";
        return 1;
    }

    if (!single) 
        db.extractChannels(chns);

    videos.reserve(channels.size() * 15);

    HINTERNET hSession = WinHttpOpen(L"WinHTTP Example/1.0",
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

    for (const auto &ch : chns)
    {
        std::vector<Video> chanVideos;
        db.loadVideosAndTrim(chanVideos, ch.name);
        videos.insert(videos.end(), chanVideos.begin(), chanVideos.end());
    }

    std::sort(videos.begin(), videos.end(), [](const auto &a, const auto &b)
              { return a > b; });

    std::wcout << L"New Videos(s): " << cv << std::endl;
    if (newchan > 0) std::wcout << L"New Channel(s): " << newchan << std::endl;
    
    for (int i = 0; i < videos.size(); i++)
    {
        if (i >= limit) break;
        videos[i].printVideo();
    }

    _setmode(_fileno(stdout), _O_TEXT);

    WinHttpCloseHandle(hSession);
    WinHttpCloseHandle(hConnect);
    CoUninitialize();

    return 0;
}