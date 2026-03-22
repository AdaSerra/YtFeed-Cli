#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <ctime>

enum Field : uint8_t
{
    TIMESTAMP = 0,
    AUTHOR = 1,
    TITLE = 2,
    VIEWS = 3,
    STARS = 4,
    PERCENT = 5
};

enum Direction : uint8_t
{
    ASC = 0,
    DESC = 1
};

enum VideoFilter : uint8_t
{
    ONLY_NORMAL = 0,
    ONLY_SHORT = 1,
    ALL = 2,
};

enum TimeFilter : uint8_t
{
    NONE = 0,
    EQUAL = 1,
    BEFORE = 2,
    AFTER = 3,
    RANGE = 4
};

struct Channel
{
    std::string id = "";
    std::string name = "";
    void printChannel() const;   
};

struct Video
{
public:
    time_t tp = 0;
    uint64_t views = 0;
    uint64_t stars = 0;
    double percent = 0.0;
    char id[12] = {};
    bool sh = true; // true normal // false short
    std::string title;
    std::string author;
    
    Video() = default;
    Video(const std::string &wi, time_t nt, const std::string &ti, const std::string &au, const std::string &idStr, bool sh);
    time_t getTime(const std::string &iso);
    void printVideo(bool ext, int idx = 0);
    void jsonVideo();
    void clear();
    
    bool operator>(const Video &other) const
    {
        if (tp == other.tp)
            return author > other.author;
        else
            return tp > other.tp;
    }
};
