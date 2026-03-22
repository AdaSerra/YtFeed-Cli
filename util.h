#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include "types.h"

struct Sqlite;
struct Channel;
struct Video;

char *extract_xml_tag(const char *src, const char *tag, char *out, size_t out_size);
char *extract_xml_attr(const char *src, const char *tag, const char *attr,
                       char *out, size_t out_size);
void parsingXml(std::string &response, std::vector<Video> &vec, Channel &ch, Sqlite &db);
size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
void readFile(std::vector<std::string> &channels, const char * filename);
std::string escapeHTML(const std::string_view data);
void generateHTML(const std::vector<Video> &videos, size_t newchan, size_t newvid, uint32_t limit);
bool parse_int(const char *str, int &out);
bool isValidFilename(const char *filename);
time_t stringToTp(const char* dateStr);

inline bool isValidChannelID(std::string &id) 
{
    
    // check len before
    if (id.size() != 24) return false;

    // check UC
    if (id[0] != 'U' || id[1] != 'C') return false;

    // check the remaining 22 chars [A-Za-z0-9_-]
    for (size_t i = 2; i < 24; ++i) {
        char c = id[i];
        bool isValid = (c >= 'A' && c <= 'Z') || 
                       (c >= 'a' && c <= 'z') || 
                       (c >= '0' && c <= '9') || 
                       (c == '_') || (c == '-');
        if (!isValid) return false;
    }

    return true;
};

inline size_t utf8CharLen(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;      // ASCII
    if ((c & 0xE0) == 0xC0) return 2;      // 2 byte
    if ((c & 0xF0) == 0xE0) return 3;      // 3 byte
    if ((c & 0xF8) == 0xF0) return 4;      // 4 byte
    return 1; // fallback 
}


