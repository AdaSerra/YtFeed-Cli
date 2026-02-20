#pragma once
#include <windows.h>
#include <iostream>
#include <codecvt>
#include <ctime>

struct Channel
{
    std::wstring id = L"";
    std::wstring name = L"";

    void printChannel() const
    {
        std::wcout << std::left << std::setw(28) << id << "   " << std::setw(28) << name << std::endl;  
    }
    
};

struct Video
{
    time_t tp = 0;
    std::wstring title;
    std::wstring author;
    std::wstring id; 

    time_t getTime(const std::wstring& wiso)
{
    struct tm tm = {};
    int tz_hour = 0, tz_min = 0;
    char sign = '+';

    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    std::string iso = conv.to_bytes(wiso);

    // Caso con offset
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

    void printVideo() const
    {
        wchar_t timeStr[64];
        struct tm* lt = localtime(&tp);
        wcsftime(timeStr, sizeof(timeStr) / sizeof(wchar_t), L"%H:%M:%S %d-%m-%Y", lt);

        std::wcout << std::left << std::setw(20) << timeStr << "   " << std::setw(28) << author.substr(0, 28) << std::setw(50) << title.substr(0, 50) << "    " << std::setw(60) << (L"https://www.youtube.com/watch?=v" + id).substr(0, 60)
                   << std::endl;
    }

    Video() = default;
    Video(const std::wstring& wi, time_t nt, const std::wstring& au, const std::wstring& ti, const std::wstring& id) : author(au), title(ti), id(id)
    {
        if (nt == 0)
            tp = getTime(wi);
        else
            tp = nt;
    }

    bool operator>(const Video& other) const
    {
        if (tp == other.tp)
        {
            return author > other.author;
        }

        else
            return tp > other.tp;
    }
    void clear() 
    {
        author.clear(); id.clear(); title.clear(); tp = 0;
    }
};