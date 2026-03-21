#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "types.h"    
#include <sqlite3.h>  

inline constexpr const char *CT_VIDEOS =
    "CREATE TABLE IF NOT EXISTS Videos "
    "(Id TEXT PRIMARY KEY, "
    "Timestamp INTEGER, "
    "Title TEXT, "
    "Author TEXT, "
    "Views INTEGER, "
    "Stars INTEGER, "
    "Short INTEGER NOT NULL CHECK (Short IN (0, 1)), " // 0 normal 1 short
    "FOREIGN KEY(Author) REFERENCES Channels(Name) ON DELETE CASCADE);";

inline constexpr const char *CT_CHANNELS =
    "CREATE TABLE IF NOT EXISTS Channels"
    " (Id TEXT PRIMARY KEY, Name TEXT UNIQUE);";

inline constexpr const char *CT_SETTING =
    "CREATE TABLE IF NOT EXISTS Setting ("
    "id INTEGER PRIMARY KEY CHECK (id = 1),"
    "Keep_Feed INTEGER DEFAULT 30,"
    "Last_updated INTEGER DEFAULT (strftime('%s', 'now'))"
    ");";
inline constexpr const char *IDX1 = "CREATE INDEX IF NOT EXISTS idx_videos_timestamp ON Videos (Timestamp DESC);";

struct Stmt
{
    sqlite3_stmt *ptr = nullptr;
    ~Stmt()
    {
        if (ptr)
            sqlite3_finalize(ptr);
    }
};

class Sqlite
{
private:
    sqlite3 *db = nullptr;
    int rc = SQLITE_OK;
    int version = 0;

    void handleError(const char *msg);
    void readVideo(Video &vid, const Stmt &st);
    void execpreset(const char *preset);
    void getCountPair(const char *sql, std::vector<std::pair<std::string, int>> &out);

    template <typename T>
    T genericQuery(const char *que);
   

public:
    Field f = TIMESTAMP;
    Direction d = DESC;
    VideoFilter vf = ALL;
    Sqlite();
    Sqlite(const char *file);
    ~Sqlite();
    void open(const char *file);
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    void saveSettings(int keepFeed = -1);
    int updateChannel(const Channel &ch);
    int insertChannel(const std::string &chid);
    int removeChannel(const std::string &name);
    bool existsChannel(const std::string &name);
    void extractChannels(std::vector<Channel> &vec);
    void extractVideos(std::vector<Video> &vec, uint32_t lim, const std::string filterChannel = "", uint32_t tsb = 0);
    void trimAllAuthor();
    int insertVideosBatch(const std::vector<Video> &videos);
    void extractVideosLast24h(std::vector<Video> &vec);
    void getVideoBoundaries(Video &out, bool desc, Field fd);
    void purge(size_t &dc, size_t &dv);
    void stat(int width);
    void close();
};

template <typename T>
T Sqlite::genericQuery(const char* que)
{
    T value{};
    Stmt st{nullptr};

    if (sqlite3_prepare_v2(db, que, -1, &st.ptr, nullptr) != SQLITE_OK)
    {
        std::cerr << "[Sqlite] Error prepare query: " << que
                  << " Error: " << sqlite3_errmsg(db) << "\n";
        return value;
    }

    rc = sqlite3_step(st.ptr);
    if (rc == SQLITE_ROW)
    {
        if constexpr (std::is_same_v<T, int>)
            value = sqlite3_column_int(st.ptr, 0);

        else if constexpr (std::is_same_v<T, long long>)
            value = sqlite3_column_int64(st.ptr, 0);

        else if constexpr (std::is_same_v<T, std::string>)
        {
            const void* txt = sqlite3_column_text(st.ptr, 0);
            value = txt ? reinterpret_cast<const char*>(txt) : "";
        }
        else
            static_assert(!sizeof(T*), "Unsupported type");
    }
    else
    {
        std::cerr << "[Sqlite] Error step on query: " << que
                  << " Error: " << sqlite3_errmsg(db) << "\n";
    }

    return value;
}
