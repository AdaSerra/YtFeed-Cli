#include <cstddef>      
#include <cstdint>      
#include <iostream>
#include <string>
#include <ctime>
#include <fstream>
#include <vector>
#define CURL_STATICLIB
#include <curl/curl.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include "const.h"
#include "util.h"
#include "types.h"
#include "sqll.h"

char *extract_xml_tag(const char *src, const char *tag, char *out, size_t out_size)
{
    char open_tag[128];
    char close_tag[128];

    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = strstr(src, open_tag);
    if (!start)
        return NULL;

    start += strlen(open_tag);

    const char *end = strstr(start, close_tag);
    if (!end)
        return NULL;

    size_t content_len = end - start;
    if (content_len >= out_size)
        content_len = out_size - 1;

    memcpy(out, start, content_len);
    out[content_len] = '\0';

    return const_cast<char *>(end + strlen(close_tag));
}

char *extract_xml_attr(const char *src, const char *tag, const char *attr,
                       char *out, size_t out_size)
{
    char open_tag[128];
    char pattern[128];

    snprintf(open_tag, sizeof(open_tag), "<%s", tag);
    snprintf(pattern, sizeof(pattern), "%s=\"", attr);

    const char *start = strstr(src, open_tag);
    if (!start)
        return NULL;

    const char *close = strstr(start, "/>");
    size_t close_len = 2;
    if (!close)
    {
        close = strstr(start, " />");
        if (!close)
            return NULL;
        close_len = 3;
    }

    const char *tag_end = close;

    const char *attr_start = strstr(start, pattern);
    if (!attr_start || attr_start > tag_end)
        return NULL;

    attr_start += strlen(pattern);

    const char *attr_end = strchr(attr_start, '"');
    if (!attr_end || attr_end > tag_end)
        return NULL;

    size_t len = (size_t)(attr_end - attr_start);
    if (len >= out_size)
        len = out_size - 1;

    memcpy(out, attr_start, len);
    out[len] = '\0';

    return const_cast<char *>(close + close_len);
}

void parsingXml(std::string &response, std::vector<Video> &vec, Channel &ch, Sqlite &db)
{
    if (response.empty())
        return;

    std::vector<char> out_vec(ENTRY_BUFFER_SIZE);
    char *out = out_vec.data();

    char sub[1024];
    const char *na = response.c_str();

    if (ch.name.empty())
    {
        if (const char *found = extract_xml_tag(na, "title", out, ENTRY_BUFFER_SIZE))
        {
            ch.name = out;
            db.updateChannel(ch);
            na = found;
        }
    }

    const char *p = na;

    // Loop on entry
    while ((p = extract_xml_tag(p, "entry", out, ENTRY_BUFFER_SIZE)))
    {
        Video v{};
        v.author = ch.name;

        const char *inner = out;

        // Video ID
        if (extract_xml_tag(inner, "yt:videoId", sub, sizeof(sub)))
        {           
            memcpy(v.id, sub, sizeof(v.id));
          //  std::cout << "extract: [" << sub << "] len=" << strlen(sub) << "\n";
            if (extract_xml_attr(inner, "link", "href", sub, sizeof(sub)))
            {
                size_t ll = strlen(sub);
                if (ll == 43)
                    v.sh = false;
                else if (ll == 42)
                    v.sh = true;
            }

            // Date
            if (extract_xml_tag(inner, "published", sub, sizeof(sub)))
            {
                v.tp = v.getTime(sub);
            }

            // Title
            if (extract_xml_tag(inner, "media:title", sub, sizeof(sub)))
            {
                v.title = sub;
            }

            // Rating
            if (extract_xml_attr(inner, "media:starRating", "count", sub, sizeof(sub)))
            {
                v.stars = strtol(sub, NULL, 10);
            }

            // Views
            if (extract_xml_attr(inner, "media:statistics", "views", sub, sizeof(sub)))
            {
                v.views = strtol(sub, NULL, 10);
            }

            vec.emplace_back(std::move(v));
        }
    }
}

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t totalSize = size * nmemb;

    std::string *buffer = static_cast<std::string *>(userp);
    buffer->append(static_cast<const char *>(contents), totalSize);
    return totalSize;
}


void readFile(std::vector<std::string> &channels, const char *filename)
{
    channels.clear();
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "[System] Error: impossibile opening file\n";
        return;
    }

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line))
    {

        if (firstLine && line.size() >= 3)
        {
            if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            {
                line.erase(0, 3);
            }
            firstLine = false;
        }

        // Trim  (O(N))
        size_t last = line.find_last_not_of(" \t\r\n");
        if (last == std::string::npos)
            continue; // empty line or ' '

        size_t first = line.find_first_not_of(" \t\r\n");

        std::string cleanLine = line.substr(first, (last - first + 1));

        if (isValidChannelID(cleanLine))
        {
            channels.push_back(std::move(cleanLine));
        }
        else
        {
            std::cerr << "[Input] Error: not valid Id [" << cleanLine << "]\n";
        }
    }
}
std::string escapeHTML(const std::string_view data)
{
    std::string buffer;
    buffer.reserve(data.size());

    for (size_t pos = 0; pos != data.size(); ++pos)
    {
        switch (data[pos])
        {
        case '&':
            buffer.append("&amp;");
            break;
        case '\"':
            buffer.append("&quot;");
            break;
        case '\'':
            buffer.append("&apos;");
            break;
        case '<':
            buffer.append("&lt;");
            break;
        case '>':
            buffer.append("&gt;");
            break;
        default:
            buffer.append(1, data[pos]);
            break;
        }
    }
    return buffer;
}

void generateHTML(const std::vector<Video> &videos, size_t newchan, size_t newvid, uint32_t limit)
{
    FILE *htmlFile = fopen("feed.html", "wb");

    if (!htmlFile)
    {
        std::wcerr << "[System] Impossible generating html file\n";
        return;
    }

    // Header
    fprintf(htmlFile, "<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'>"
                      "<link rel='icon' type='image/png' href='logo3.png'>"
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

    fprintf(htmlFile, "<h1>YouTube Feed Update</h1>");
    fprintf(htmlFile, "<div class='stats'>New Videos: %zu  | New Channel: %zu</div>", newchan, newvid);

    fprintf(htmlFile, "<div class='grid'>");

    // Loop video
    for (uint32_t i = 0; i < videos.size() && i < limit; i++)
    {
        // escpape
        std::string safeTitle = escapeHTML(videos[i].title);
        std::string safeAuthor = escapeHTML(videos[i].author);

        fprintf(htmlFile, "<div class='card'>"
                          "<a href='%s%s' target='_blank'>"
                          "<img src='https://i.ytimg.com/vi/%s/mqdefault.jpg'>"
                          "<div class='info'>"
                          "<h4>%s</h4>"
                          "<p class='author'>%s</p>"
                          "</div></a></div>",
                YTURL_FULL, videos[i].id, videos[i].id, safeTitle.c_str(), safeAuthor.c_str());
    }
    fprintf(htmlFile, "</div></body></html>");

    fclose(htmlFile);

#ifdef _WIN32
    const char *cmd = "start feed.html";
#elif __APPLE__
    const char *cmd = "open feed.html";
#else
    const char *cmd = "xdg-open feed.html";
#endif
    system(cmd);
}



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

    size_t len = strlen(filename);
    if (len == 0)
        return false;

    // refuse "." ".."
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
        return false;

    // Windows not valid characters
    const char *invalid = "<>:\"/\\|?*";

    for (size_t i = 0; i < len; i++)
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

#ifdef _WIN32
    #define stricmp _stricmp
#else
  
    #define stricmp strcasecmp
#endif
    for (int i = 0; i < 22; i++)
    {
        if (stricmp(base, reserved[i]) == 0)
            return false;
    }

    // check extension
    const char *last_dot = strrchr(base, '.');
    if (!last_dot || last_dot == base)
        return false;

    const char *ext = last_dot + 1;
    size_t ext_len = strlen(ext);
    if (ext_len < 1 || ext_len > 4)
        return false;

    for (int i = 0; i < ext_len; i++)
    {
        if (!isalnum((unsigned char)ext[i]))
            return false;
    }

    return true;
}

time_t stringToTp(const char* dateStr) 
{
   struct tm t;
    memset(&t, 0, sizeof(t));

    int day, month, year;

    if (sscanf(dateStr, "%d-%d-%d", &day, &month, &year) != 3)
        return (time_t)-1;

    t.tm_mday = day;
    t.tm_mon  = month - 1;  
    t.tm_year = year - 1900;

    return mktime(&t);
}


/* void printLeftPaddedUTF8(std::string_view input, size_t width, size_t max_chars)
{
    size_t visualWidth = 0;
    size_t charsCount  = 0;
    size_t i = 0;
    size_t len = input.size();

    std::string output;
    output.reserve(len);

    while (i < len && charsCount < max_chars)
    {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // 1. HTML entities 
        if (c == '&')
        {
            size_t semi = input.find(';', i);
            if (semi != std::string_view::npos && (semi - i) < 8)
            {
                std::string_view entity = input.substr(i, semi - i + 1);
                std::string_view decoded;

                if (entity == "&quot;")      decoded = "\"";
                else if (entity == "&amp;")  decoded = "&";
                else if (entity == "&apos;") decoded = "'";
                else if (entity == "&lt;")   decoded = "<";
                else if (entity == "&gt;")   decoded = ">";
                else if (entity == "&eacute;") decoded = "é";
                else if (entity == "&agrave;") decoded = "à";

                if (!decoded.empty())
                {
                    output += decoded;
                    visualWidth += 1;
                    charsCount++;
                    i = semi + 1;
                    continue;
                }
            }
        }

        // 2. Sanification
        if (c < 32)
        {
            i++;
            continue;
        }

        // 3. UTF‑8 detection
        size_t charLen = 1;
        bool isWide = false;

        if (c >= 0xC0)
        {
            if (c >= 0xF0) { charLen = 4; isWide = true; }
            else if (c >= 0xE0) charLen = 3;
            else charLen = 2;
        }

        if (i + charLen > len)
            charLen = len - i;

        // 4. Append UTF‑8 char
        output.append(input.substr(i, charLen));

        visualWidth += (isWide ? 2 : 1);
        charsCount++;
        i += charLen;
    }

    // 5. Padding left
    if (visualWidth < width)
    {
        size_t pad = width - visualWidth;
        for (size_t p = 0; p < pad; ++p)
            std::cout.put(' ');
    }

    std::cout << output;
}
 */
