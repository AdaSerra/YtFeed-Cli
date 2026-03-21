
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
#include "types.h"
#include "console.h"

void Channel::printChannel() const
{
    std::cout << std::left << std::setw(28) << id << "   " << std::setw(28) << name << "\n";
}

time_t Video::getTime(const std::string &iso)
{
    struct tm tm = {};
    int tz_hour = 0, tz_min = 0;
    char sign = '+';

    // with offset
    if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
               &sign, &tz_hour, &tz_min) == 9)
    {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;

        time_t t = mktime(&tm);

        int offset = tz_hour * 3600 + tz_min * 60;
        if (sign == '+')
            t -= offset;
        else if (sign == '-')
            t += offset;

        return t;
    }
    // without offset
    else if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6)
    {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;

        return mktime(&tm); // local time
    }
    else
    {
        return (time_t)-1; // error
    }
}

Video::Video(const std::string &wi, time_t nt, const std::string &ti, const std::string &au, const std::string &idStr, bool sh) : title(ti), author(au), sh(sh)
{
    if (nt == 0)
        tp = getTime(wi);
    else
        tp = nt;

    strncpy(id, idStr.c_str(), sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
}

void Video::printVideo(bool ext, int idx)
{
    char timeStr[22];
    struct tm lt;

#ifdef _WIN32
    localtime_s(&lt, &tp);
#else
    localtime_r(&tp, &lt);
#endif

    strftime(timeStr, sizeof(timeStr), "%H:%M:%S %d-%m-%Y", &lt);

    if (ext)
    {
        std::cout << std::left << std::setw(4) << idx;
    }

    std::cout << timeStr << "  ";

    printLeftPadded(author, 22);
    std::cout.put(' ');
    printLeftPadded(title, 52);

    if (ext)
    {
        std::cout << std::right
                  << std::setw(10) << views
                  << std::setw(11) << stars << " ";
    }
    else
    {
        std::cout << "       ";
    }

    std::cout << " https://youtu.be/" << id;

    if (ext && sh)
        std::cout << "  S";

    std::cout << '\n';
}
/*  void printVideoExt(int idx)
 {
     char timeStr[64];
     struct tm *lt = localtime(&tp);
     strftime(timeStr, sizeof(timeStr), "%H:%M:%S %d-%m-%Y", lt);

     // author e title potrebbero essere più lunghi: li tronchiamo
     std::string s_author = sanitize(author, 21);
     std::string s_title = sanitize(title, 51);
     const char *C_RESET = "\033[0m";
     const char *C_IDX = "\033[38;5;245m";
     const char *C_TIME = "\033[38;5;111m";
     const char *C_AUTH = "\033[38;5;190m";
     const char *C_TITLE = "\033[38;5;39m";
     const char *C_NUM = "\033[38;5;208m";
     const char *C_URL = "\033[38;5;82m";

     printf("%-4d %-19s %-22.21s %-52.51s %10u %8u https://youtu.be/%-11s%s\n",
            idx,
            timeStr,
            author.c_str(),
            title.c_str(),
            views,
            stars,
            id.c_str(),
            sh ? "  S" : "");
 } */

void Video::clear()
{
    author.clear();
    memset(id, 0, sizeof(id));
    title.clear();
    tp = 0;
    views = 0;
    stars = 0;
}
