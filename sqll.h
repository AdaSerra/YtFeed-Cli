#pragma once
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <winsqlite/winsqlite3.h>

#include <iostream>
#include <string>
#include <vector>

#include "types.h"

inline constexpr const char *CT_VIDEOS =
    "CREATE TABLE IF NOT EXISTS Videos "
    "(Id TEXT PRIMARY KEY, "
    "Timestamp INTEGER, "
    "Title TEXT, "
    "Author TEXT, "
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

class Sqlite
{
private:
    sqlite3 *db = nullptr;
    int rc = SQLITE_OK;
    sqlite3_stmt *stmt = nullptr;
    std::string query;

    void handleError(const wchar_t *msg)
    {
        std::wcerr << msg << L": "
                   << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                   << std::endl;

        // rollback only if a transaction is active
        if (sqlite3_get_autocommit(db) == 0)
            rollbackTransaction();
    }

    void extractVideos(std::vector<Video> &vec)
    {

        std::wstring lastVideoId;
        Video currentVideo;
        bool firstVideo = true;

        // 2. for every row
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {

            const void *idPtr = sqlite3_column_text16(stmt, 0);
            std::wstring currentVideoId = idPtr ? reinterpret_cast<const wchar_t *>(idPtr) : L"";

            // -----------------------------------------------------
            // new aggreation logic
            if (currentVideoId != lastVideoId)
            {

                // if not first video, push in video in final vector

                currentVideo.clear();

                // read data new video
                currentVideo.tp = sqlite3_column_int64(stmt, 1);
                const void *title = sqlite3_column_text16(stmt, 2);
                const void *author = sqlite3_column_text16(stmt, 3);
                sqlite3_column_int64(stmt, 4) == 0 ? currentVideo.sh = false : currentVideo.sh = true;
                // const void* url = sqlite3_column_text16(stmt, 9);

                // wstring conversion
                currentVideo.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
                currentVideo.id = currentVideoId;
                currentVideo.author = author ? reinterpret_cast<const wchar_t *>(author) : L"";

                // update
                lastVideoId = currentVideoId;
                firstVideo = false;
            }

            // -----------------------------------------------------
        }

        sqlite3_finalize(stmt);

        // add last video in the buffer
        if (!firstVideo)
            vec.emplace_back(std::move(currentVideo));
    }

    template <typename T>
    T genericQuery(const char *que)
    {
        T value{}; // default: 0  int, "" string

        if (sqlite3_prepare_v2(db, que, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::wcerr << L"Error prepare query: " << que << " Error: " << sqlite3_errmsg(db) << "\n";
            return value;
        }

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {

            if constexpr (std::is_same_v<T, int>)
                value = sqlite3_column_int(stmt, 0);

            else if constexpr (std::is_same_v<T, long long>)
                value = sqlite3_column_int64(stmt, 0);

            else if constexpr (std::is_same_v<T, std::wstring>)
            {
                const void *txt = sqlite3_column_text16(stmt, 0);
                if (txt)
                    value = reinterpret_cast<const wchar_t *>(txt);
                else
                    value = L"";
            }
            else
                static_assert(!sizeof(T *), "Type not supported in generic query Function");
        }
        else
            std::wcerr << L"Error step on query: " << que << " Error: " << sqlite3_errmsg(db) << "\n";

        sqlite3_finalize(stmt);
        return value;
    }

    void getCountPair(const char *sql, std::vector<std::pair<std::wstring, int>> &out)
    {
        out.clear();

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {

            const void *txt = sqlite3_column_text16(stmt, 0);
            std::wstring author;

            if (txt)
            {
                author = reinterpret_cast<const wchar_t *>(txt);
            }

            int count = sqlite3_column_int(stmt, 1);
            out.emplace_back(author, count);
        }

        sqlite3_finalize(stmt);
    }

public:
    Sqlite() = default;
    Sqlite(const std::wstring &file) { open(file); }

    void open(const std::wstring &file)
    {

        rc = sqlite3_open16(file.c_str(), &db);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error Opening Database");
            db = nullptr;
        }
        sqlite3_exec(db, "INSERT OR IGNORE INTO Setting (id, Keep_Feed) VALUES (1, 30);", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    }

    ~Sqlite() { close(); }

    void beginTransaction()
    {
        char *err = nullptr;
        if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::cerr << "BEGIN failed: " << err << "\n";
            sqlite3_free(err);
        }
    }

    void commitTransaction()
    {
        char *err = nullptr;
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::wcerr << L"COMMIT failed: " << err << L"\n";
            sqlite3_free(err);
        }
    }

    void rollbackTransaction()
    {
        char *err = nullptr;
        if (sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::wcerr << L"ROLLBACK failed: " << err << L"\n";
            sqlite3_free(err);
        }
    }

    void saveSettings(int keepFeed = -1)
    {

        query =
            "INSERT INTO Setting (id, Keep_Feed, Last_updated) "
            "VALUES (1, CASE WHEN ?1 = -1 THEN 30 ELSE ?1 END, strftime('%s', 'now')) "
            "ON CONFLICT(id) DO UPDATE SET "
            "Keep_Feed = CASE WHEN ?1 = -1 THEN Keep_Feed ELSE ?1 END, "
            "Last_updated = strftime('%s', 'now');";

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::wcerr << L"Prepare Error: " << (const wchar_t *)sqlite3_errmsg16(db) << std::endl;
            return;
        }

        sqlite3_bind_int(stmt, 1, keepFeed);

        int rc = sqlite3_step(stmt);

        if (rc != SQLITE_DONE)
            handleError(L"Error saveSetting:");

        sqlite3_finalize(stmt);
    }

    void createTable(const char *preset)
    {
        rc = sqlite3_exec(db, preset, nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
            handleError(L"Error creating table");
        return;
    }

    int updateChannel(const Channel &ch)
    {
        query = "UPDATE Channels SET Name =? WHERE ID = ?;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Errore prepare (updateChannel) ");
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, ch.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(stmt, 2, ch.id.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            handleError(L"Errore updateChannel ");
            return 0;
        }

        sqlite3_finalize(stmt);
        return 1;
    }

    int insertChannel(const std::wstring &chid)
    {

        if (chid.empty())
            return 0;

        int counter = 0;

        query =
            "INSERT INTO Channels (Id) VALUES (?) "
            "ON CONFLICT(Id) DO NOTHING;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Errore prepare insertChannel: "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, chid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE)
            counter = sqlite3_changes(db); // 1 = insert, 0 = already present

        sqlite3_finalize(stmt);
        return counter;
    }

    int removeChannel(const std::wstring &name)
    {
        beginTransaction();
        query = "DELETE FROM Channels WHERE NAME = ?;";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare (removeChannel) ");
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            handleError(L"Error removeChannel ");
            return 0;
        }

        sqlite3_finalize(stmt);
        if (sqlite3_changes(db) > 0)
        {
            saveSettings(-1);
            commitTransaction();
            return 1;
        }
        commitTransaction();
        return 0;
    }

    int extractSingleChannel(std::vector<Channel> &vec, const std::wstring &name)
    {
        vec.clear();
        query = "SELECT Channels.Id, Channels.Name FROM Channels WHERE NAME = ?;";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Error prepare extractSIngleChannel: " << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db)) << std::endl;
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const void *id = sqlite3_column_text16(stmt, 0);
            const void *name = sqlite3_column_text16(stmt, 1);

            Channel ch{};
            ch.id = id ? std::wstring(reinterpret_cast<const wchar_t *>(id)) : L"";
            ch.name = name ? std::wstring(reinterpret_cast<const wchar_t *>(name)) : L"";
            vec.emplace_back(ch);
        }

        sqlite3_finalize(stmt);

        return 0;
    }

    void extractChannels(std::vector<Channel> &vec)
    {
        vec.clear();
        query = "SELECT Channels.Id, Channels.Name FROM Channels ORDER BY Channels.Name COLLATE NOCASE;";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare extractChannels:");
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const void *id = sqlite3_column_text16(stmt, 0);
            const void *name = sqlite3_column_text16(stmt, 1);

            Channel ch{};
            ch.id = id ? std::wstring(reinterpret_cast<const wchar_t *>(id)) : L"";
            ch.name = name ? std::wstring(reinterpret_cast<const wchar_t *>(name)) : L"";
            vec.emplace_back(ch);
        }

        sqlite3_finalize(stmt);
        return;
    }

    void loadVideosAndTrim(std::vector<Video> &vec, const std::wstring &author)
    {
        int limit = genericQuery<int>("SELECT Keep_Feed FROM Setting WHERE id = 1;");

        vec.clear();

        // --- 1) QUERY SELECT ---
        query = "SELECT Timestamp, Title, Author, Id, Short FROM Videos WHERE Author = ? ORDER BY Timestamp DESC;";

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            handleError(L"Error prepare (loadVideosAndTrim)");
            return;
        }

        sqlite3_bind_text16(stmt, 1, author.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Video v{};
            v.tp = sqlite3_column_int64(stmt, 0);

            const void *title = sqlite3_column_text16(stmt, 1);
            const void *auth = sqlite3_column_text16(stmt, 2);
            const void *vidId = sqlite3_column_text16(stmt, 3);

            v.sh = (sqlite3_column_int(stmt, 4) == 1);

            v.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
            v.author = auth ? reinterpret_cast<const wchar_t *>(auth) : L"";
            v.id = vidId ? reinterpret_cast<const wchar_t *>(vidId) : L"";

            vec.emplace_back(std::move(v));
        }
        sqlite3_finalize(stmt);

        // --- 2) TRIMMING ---
        if (vec.size() > (size_t)limit)
        {
              beginTransaction();

            query = "DELETE FROM Videos WHERE Author = ? AND Timestamp < ?;";
       
            rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK)
            {
                handleError(L"Error prepare (delete videos over limit)");
                return;
            }

            // Il cutoff last timestamp video
            long long cutoff = vec[limit - 1].tp;

            sqlite3_bind_text16(stmt, 1, author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, cutoff);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                handleError(L"Error executing delete trim");
            }

            sqlite3_finalize(stmt);

            saveSettings(-1); 
            commitTransaction();

            vec.resize(limit);
        }
    }

    int insertVideosBatch(const std::vector<Video> &videos)
    {
        beginTransaction();
        int counter = 0;

        query = "INSERT OR IGNORE INTO Videos (Id, Timestamp, Title, Author, Short) VALUES (?, ?, ?, ?, ?);";

        stmt = nullptr;
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);

        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare Batch UPSERT");
            return 0;
        }

        for (const auto &vid : videos)
        {
            // bind
            sqlite3_bind_text16(stmt, 1, vid.id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, vid.tp);
            sqlite3_bind_text16(stmt, 3, vid.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text16(stmt, 4, vid.author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 5, vid.sh ? 1 : 0);

            rc = sqlite3_step(stmt);

            if (rc == SQLITE_DONE)
            {
                if (sqlite3_changes(db) > 0)
                    counter++;
            }
            else
            {
                sqlite3_finalize(stmt);
                handleError(L"Error during batch step");
            }

            // Reset
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }

        sqlite3_finalize(stmt);
        if (counter > 0)
            saveSettings(-1);
        commitTransaction();

        return counter;
    }

    int insertVideo(const Video &vid)
    {
        // Query with UPSERT
        query =
            "INSERT INTO Videos (Id, Timestamp, Title, Author, Short) "
            "VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(Id) DO UPDATE SET "
            "Timestamp = excluded.Timestamp, "
            "Title = excluded.Title, "
            "Author = excluded.Author, "
            "Short = excluded.Short;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Error prepare (insertVideo): "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            return 0;
        }

        // Bind
        sqlite3_bind_text16(stmt, 1, vid.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, vid.tp);
        sqlite3_bind_text16(stmt, 3, vid.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(stmt, 4, vid.author.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, vid.sh ? 1 : 0);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            std::wcerr << L"Error upsert(insertVideo): "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            sqlite3_finalize(stmt);
            return 0;
        }

        sqlite3_finalize(stmt);
        return 1;
    }

    void extractVideosLast24h(std::vector<Video> &vec)
    {
        vec.clear();
        const char *sql =
            "SELECT Timestamp, Title, Author, Id, Short FROM Videos "
            "WHERE Timestamp > strftime('%s','now') - 86400;";

        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare (extractVideo24h): ");
            return;
        }

        // --- EXTRACT ---
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Video v{};

            v.tp = sqlite3_column_int64(stmt, 0);
            const void *title = sqlite3_column_text16(stmt, 1);
            const void *author = sqlite3_column_text16(stmt, 2);
            const void *id = sqlite3_column_text16(stmt, 3);
            sqlite3_column_int64(stmt, 4) == 1 ? v.sh = true : v.sh = false;

            // wstring conversion
            v.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
            v.id = id ? reinterpret_cast<const wchar_t *>(id) : L"";
            v.author = author ? reinterpret_cast<const wchar_t *>(author) : L"";

            vec.emplace_back(std::move(v));
        }
        sqlite3_finalize(stmt);
    }

    void getVideoBoundaries(Video &out, bool desc)
    {
        query =
            "SELECT Id, Timestamp, Title, Author, Short FROM Videos ORDER BY Timestamp ";
        if (desc)
            query += "ASC";
        else
            query += "DESC";

        " LIMIT 1;";

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            handleError(L"Error prepare getVideoBoundaries: ")
            return;
        }

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const void *id = sqlite3_column_text16(stmt, 0);
            const void *title = sqlite3_column_text16(stmt, 2);
            const void *author = sqlite3_column_text16(stmt, 3);

            out.id = id ? reinterpret_cast<const wchar_t *>(id) : L"";
            out.tp = sqlite3_column_int64(stmt, 1);
            out.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
            out.author = author ? reinterpret_cast<const wchar_t *>(author) : L"";
            out.sh = sqlite3_column_int(stmt, 4) == 1;

            sqlite3_finalize(stmt);
            return;
        }

        sqlite3_finalize(stmt);
        return;
    }

    void stat()
    {
        FILE *f = fopen("local.db", "rb");
        if (!f)
        {
            std::wcout << L"Not database found\n";
            return;
        }

        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fclose(f);

        int totalVideos = genericQuery<int>("SELECT COUNT(*) FROM Videos;");
        int totalChannels = genericQuery<int>("SELECT COUNT(*) FROM Channels;");
        int keep = genericQuery<int>("SELECT Keep_Feed FROM Setting WHERE id = 1;");
        int last24h = genericQuery<int>("SELECT COUNT(*) FROM Videos WHERE Timestamp > strftime('%s','now') - 86400;");
        int last7d = genericQuery<int>("SELECT COUNT(*) FROM Videos WHERE Timestamp > strftime('%s','now') - 604800;");
        int last2m = genericQuery<int>("SELECT COUNT(*) FROM Videos WHERE Timestamp > strftime('%s','now','-56 days');");
        int avgGap = genericQuery<int>(
            "SELECT AVG(diff) FROM ("
            "  SELECT Timestamp - LAG(Timestamp) OVER (ORDER BY Timestamp) AS diff "
            "  FROM Videos"
            ");");

        int version = genericQuery<int>("PRAGMA schema_version;");
        int freelist = genericQuery<int>("PRAGMA freelist_count;");
        int syncr = genericQuery<int>("PRAGMA synchronous;");
        int tables = genericQuery<int>("SELECT COUNT(*) FROM sqlite_master WHERE type='table'");
        int countShort = genericQuery<int>("SELECT COUNT(*) FROM Videos WHERE Short = 1;");
        std::wstring formattedTime = genericQuery<std::wstring>("SELECT strftime('%H:%M %d-%m-%Y', Last_updated, 'unixepoch') FROM Setting WHERE id = 1;");
        std::wstring result = genericQuery<std::wstring>("PRAGMA integrity_check;");
        std::wstring mode = genericQuery<std::wstring>("PRAGMA journal_mode;");
        int fkeys = genericQuery<int>("PRAGMA foreign_keys;");

        Video v1, v2;
        getVideoBoundaries(v1, true);
        getVideoBoundaries(v2, false);

        std::wcout << L"\n---------- YOUTUBE STATISTICS -----------\n\n"
                   << L"Total Channels: " << totalChannels << "\n"
                   << L"Total Videos: " << totalVideos << "\n"
                   << L"Total Videos Shorts: " << countShort << "\n"
                   << L"Recent Videos (last 24 hours): " << last24h << "\n"
                   << L"Average gap between videos: " << avgGap / 3600.0 << " hours\n";

        std::vector<std::pair<std::wstring, int>> lastWeek;
        std::vector<std::pair<std::wstring, int>> lastWeekC;

        getCountPair("SELECT strftime('%d-%m-%Y', Timestamp, 'unixepoch'), COUNT(*) "
                     "FROM Videos "
                     "WHERE Timestamp >= strftime('%s','now') - 864000 "
                     "GROUP BY 1 ORDER BY 1 DESC LIMIT 10;",
                     lastWeek);
        getCountPair(
            "SELECT Author, COUNT(*) FROM Videos "
            "WHERE Timestamp >= strftime('%s','now') - 604800 "
            "GROUP BY Author ORDER BY COUNT(*) DESC LIMIT 10;",
            lastWeekC);

        std::wcout << L"\n----------- ACTIVITY LAST 10 DAYS (" << last7d << std::left << std::setw(47) << L" Videos) ------------------------- "
                   << L"---------------- TOP CHANNELS ------------------- \n\n";
        for (int i = 0; i < lastWeek.size(); i++)
        {
            std::wstring bar1(lastWeek[i].second, L'#');
            std::wstring bar2(lastWeekC[i].second, L'#');
            std::wcout << std::setw(10) << lastWeek[i].first
                       << std::setw(73) << (L" | " + bar1 + L" (" + std::to_wstring(lastWeek[i].second) + L")");
            if (i < lastWeekC.size())
                std::wcout << std::setw(25) << lastWeekC[i].first << L" | " << bar2 << L" (" << lastWeekC[i].second << ")\n";
        }

        getCountPair(
            "SELECT strftime('%d-%m-%Y', Timestamp, 'unixepoch', 'weekday 1', '-7 days') || ' - ' || "
            "strftime('%d-%m-%Y', Timestamp, 'unixepoch', 'weekday 1', '-1 day') AS WeekRange, "
            " COUNT(*)"
            "FROM Videos "
            "WHERE Timestamp >= strftime('%s','now','-56 days') "
            "GROUP BY WeekRange ORDER BY 1 DESC LIMIT 10;",
            lastWeek);

        getCountPair(
            "SELECT Author, COUNT(*) "
            "FROM Videos "
            "WHERE Timestamp >= strftime('%s','now','-56 days') "
            "GROUP BY Author ORDER BY COUNT(*) DESC LIMIT 10;",
            lastWeekC);

        std::wcout << L"\n----------- ACTIVITY LAST 2 MONTHS Weekly (" << last2m << std::setw(37) << L" Videos) ---------------- "
                   << L"---------------- TOP CHANNELS ------------------- \n\n";
        for (int i = 0; i < lastWeek.size(); i++)
        {
            std::wstring bar1((lastWeek[i].second) / 2, L'#');
            std::wstring bar2(lastWeekC[i].second, L'#');
            std::wcout << std::setw(10) << lastWeek[i].first
                       << std::setw(60) << (L" | " + bar1 + L" (" + std::to_wstring(lastWeek[i].second) + L")");
            if (i < lastWeekC.size())
                std::wcout << std::setw(25) << lastWeekC[i].first << L" | " << bar2 << L" (" << lastWeekC[i].second << ")\n";
        }

        std::wcout << L"\n-------------------------------------------------------------- NEWEST AND OLDEST VIDEOS -----------------------------------------------------------------\n\n";
        v2.printVideo();
        v1.printVideo();

        std::wcout << L"\n----------- DATABASE STATISTICS ----------------\n\n"
                   << L"Database size: " << size / 1024 << " kb\n"
                   << L"Last change: " << formattedTime.c_str() << "\n"
                   << L"Schema version: " << version << "\n"
                   << L"Integrity: " << result.c_str() << "\n"
                   << L"Freelist pages: " << freelist << "\n"
                   << L"Journal mode: " << mode.c_str() << "\n"
                   << L"Synchronous mode: " << syncr << "\n"
                   << L"Foreign Keys: " << fkeys << "\n"
                   << L"Limit Videos Saved per Channel: " << keep << "\n"
                   << L"Total table: " << tables << "\n\n";
    }

    void close()
    {
        if (db)
        {
            sqlite3_close(db);
            db = nullptr;
        }
    }
};