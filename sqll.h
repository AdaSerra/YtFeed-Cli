#pragma once
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <winsqlite/winsqlite3.h>

#include <iostream>
#include <string>
#include <vector>

#include "types.h"

inline const std::string CT_VIDEOS =
    "CREATE TABLE IF NOT EXISTS Videos "
    "(Id TEXT PRIMARY KEY, "
    "Timestamp INTEGER, "
    "Title TEXT, "
    "Author TEXT, "
    "FOREIGN KEY(Author) REFERENCES Channels(Id) );";

inline const std::string CT_CHANNELS =
    "CREATE TABLE IF NOT EXISTS Channels"
    " (Id TEXT PRIMARY KEY, Name TEXT UNIQUE); ";

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

        // rollback solo se una transazione è attiva
        if (sqlite3_get_autocommit(db) == 0)
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
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

                currentVideo.clear(); // <--- QUESTA È LA FIX

                // read data new video
                currentVideo.tp = sqlite3_column_int64(stmt, 1);
                const void *title = sqlite3_column_text16(stmt, 2);
                const void *author = sqlite3_column_text16(stmt, 3);
                // const void* url = sqlite3_column_text16(stmt, 9);

                // wstring conversion
                currentVideo.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
                currentVideo.id = currentVideoId; // url ? reinterpret_cast<const wchar_t*>(url) : L"";
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

public:
    int offset = 0;
    int pageSize = 50;
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
    }

    ~Sqlite() { close(); }

    void beginTransaction()  { sqlite3_exec(db, "BEGIN;",  nullptr, nullptr, nullptr); }
    void commitTransaction() { sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr); }


    void createTable(const std::string &preset)
    {
        rc = sqlite3_exec(db, preset.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK)
            handleError(L"Error creating table");
    }

    void nextPage() { offset += pageSize; }

    void resetPage() { offset = 0; }

    bool findChannel(const std::wstring &id)
    {
        query = "SELECT 1 FROM Channels WHERE Id = ?;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare");
            return false;
        }
        sqlite3_bind_text16(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        if (exists)
            return true;
        return false;
    }

    Channel extractChannelByAuthor(const std::wstring &name)
    {

        Channel chan{};
        std::string query = "SELECT Id FROM Channels WHERE Name = ?;";

        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            handleError(L"Error prepare extractChannelByAuthor");
            return chan; 
        }

        sqlite3_bind_text16(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const void *text = sqlite3_column_text16(stmt, 0);

            if (text)
            {
                chan.id = reinterpret_cast<const wchar_t *>(text);
                chan.name = name;
            }
        }

        sqlite3_finalize(stmt);

        return chan;
    }

    int updateChannel(const Channel &ch)
    {
        query = "UPDATE Channels SET Name =? WHERE ID = ?;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Errore prepare (update): " << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db)) << std::endl;
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, ch.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(stmt, 2, ch.id.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            std::wcerr << L"Error prepare: " << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db)) << std::endl;
            return 0;
        }

        sqlite3_finalize(stmt);
        return 1;
    }

    int insertChannel(const std::wstring& chid)
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
        query = "DELETE FROM Channels WHERE NAME = ?;";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Error prepare (delete): " << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db)) << std::endl;
            return 0;
        }

        sqlite3_bind_text16(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            std::wcerr << L"Error delete: " << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db)) << std::endl;
            return 0;
        }

        sqlite3_finalize(stmt);
        if (sqlite3_changes(db) > 0)
        {
            return 1;
        }
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
        query = "SELECT Channels.Id, Channels.Name FROM Channels;";
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Error prepare extractChannels: "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
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

        vec.clear();
        // --- 1) QUERY ---
        query =
            "SELECT Timestamp, Title, Author, Id "
            "FROM Videos "
            "WHERE Author = ? "
            "ORDER BY Timestamp DESC;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Errore prepare (loadVideosAndTrim): "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            return;
        }

        sqlite3_bind_text16(stmt, 1, author.c_str(), -1, SQLITE_TRANSIENT);

        // --- 2) EXTRACT ---
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Video v{};

            v.tp = sqlite3_column_int64(stmt, 0);
            const void *title = sqlite3_column_text16(stmt, 1);
            const void *author = sqlite3_column_text16(stmt, 2);
            const void *id = sqlite3_column_text16(stmt, 3);

            // wstring conversion
            v.title = title ? reinterpret_cast<const wchar_t *>(title) : L"";
            v.id = id ? reinterpret_cast<const wchar_t *>(id) : L"";
            v.author = author ? reinterpret_cast<const wchar_t *>(author) : L"";

            vec.push_back(std::move(v));
        }

        sqlite3_finalize(stmt);

        // --- 3) DELETE OVER 30 VIDEOS ---
        if (vec.size() > 30)
        {
            query =
                "DELETE FROM Videos "
                "WHERE Author = ? AND Timestamp < ?;";

            sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);

            // Timestamp limit =  30th recent video
            time_t cutoff = vec[29].tp;

            sqlite3_bind_text16(stmt, 1, author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, cutoff);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            // only 30
            vec.resize(30);
        }

        return;
    }

    int insertVideosBatch(const std::vector<Video> &videos)
    {
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        int counter = 0;

        // Statement INSERT with RETURNING
        const char *insertQuery =
            "INSERT INTO Videos (Id, Timestamp, Title, Author) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(Id) DO NOTHING "
            "RETURNING 1 AS was_insert;";

        sqlite3_stmt *insertStmt = nullptr;
        sqlite3_prepare_v2(db, insertQuery, -1, &insertStmt, nullptr);

        // Statement UPDATE
        const char *updateQuery =
            "UPDATE Videos SET "
            "Timestamp=?, Title=?, Author=? "
            "WHERE Id=?;";

        sqlite3_stmt *updateStmt = nullptr;
        sqlite3_prepare_v2(db, updateQuery, -1, &updateStmt, nullptr);

        for (const auto &vid : videos)
        {
            auto bindText = [&](sqlite3_stmt *s, int idx, const std::wstring &val)
            {
                if (!val.empty())
                    sqlite3_bind_text16(s, idx, val.c_str(), -1, SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(s, idx);
            };

            // ---------------------------
            // 1)  INSERT
            // ---------------------------
            bindText(insertStmt, 1, vid.id);
            sqlite3_bind_int64(insertStmt, 2, vid.tp);
            bindText(insertStmt, 3, vid.title);
            bindText(insertStmt, 4, vid.author);

            int rc = sqlite3_step(insertStmt);

            bool wasInsert = false;

            if (rc == SQLITE_ROW)
            {
                wasInsert = true;
                counter++;
            }
            else if (rc == SQLITE_DONE)
            {
                // 0 row → it was an  UPDATE
                wasInsert = false;
            }
            else
            {
                std::wcerr << L"Errore INSERT: "
                           << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                           << std::endl;
            }

            sqlite3_reset(insertStmt);
            sqlite3_clear_bindings(insertStmt);

            // ---------------------------
            // 2) if not INSERT → UPDATE
            // ---------------------------
            if (!wasInsert)
            {
                sqlite3_bind_int64(updateStmt, 1, vid.tp);
                bindText(updateStmt, 2, vid.title);
                bindText(updateStmt, 3, vid.author);

                rc = sqlite3_step(updateStmt);

                if (rc != SQLITE_DONE)
                {
                    std::wcerr << L"Errore UPDATE: "
                               << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                               << std::endl;
                }

                sqlite3_reset(updateStmt);
                sqlite3_clear_bindings(updateStmt);
            }
        }

        sqlite3_finalize(insertStmt);
        sqlite3_finalize(updateStmt);
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

        return counter;
    }

    int insertVideo(const Video &vid)
    {
        // Query with UPSERT
        query =
            "INSERT INTO Videos (Id, Timestamp, Title, Author) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(Id) DO UPDATE SET "
            "Timestamp = excluded.Timestamp, "
            "Title = excluded.Title, "
            "Author = excluded.Author;";

        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::wcerr << L"Errore prepare (UPSERT): "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            return 0;
        }

        // Bind 
        sqlite3_bind_text16(stmt, 1, vid.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, vid.tp);
        sqlite3_bind_text16(stmt, 3, vid.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text16(stmt, 4, vid.author.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            std::wcerr << L"Errore UPSERT: "
                       << reinterpret_cast<const wchar_t *>(sqlite3_errmsg16(db))
                       << std::endl;
            sqlite3_finalize(stmt);
            return 0;
        }

        sqlite3_finalize(stmt);
        return 1;
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