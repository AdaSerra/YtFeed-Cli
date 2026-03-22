
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "sqll.h"
#include "types.h"
#include "console.h"

Sqlite::Sqlite() = default;
Sqlite::Sqlite(const char *file) { open(file); }
Sqlite::~Sqlite() { close(); }

void Sqlite::handleError(const char *msg)
{
    std::cerr << "[Sqlite] " << msg << ": " << sqlite3_errmsg(db) << std::endl;

    // rollback only if a transaction is active
    if (sqlite3_get_autocommit(db) == 0)
        rollbackTransaction();
}

void Sqlite::readVideo(Video &vid, const Stmt &st)
{
    snprintf(vid.id, sizeof(vid.id), "%s", sqlite3_column_text(st.ptr, 0));

    // const void *id = sqlite3_column_text(st.ptr, 0);
    const void *title = sqlite3_column_text(st.ptr, 2);
    const void *author = sqlite3_column_text(st.ptr, 3);

    // vid.id = id ? reinterpret_cast<const char *>(id) : "";

    vid.tp = sqlite3_column_int64(st.ptr, 1);
    vid.title = title ? reinterpret_cast<const char *>(title) : "";
    vid.author = author ? reinterpret_cast<const char *>(author) : "";
    vid.views = sqlite3_column_int64(st.ptr, 4);
    vid.stars = sqlite3_column_int64(st.ptr, 5);
    vid.sh = sqlite3_column_int64(st.ptr, 6) != 0;

    if (f == PERCENT)
        vid.percent = sqlite3_column_double(st.ptr, 7);
}

void Sqlite::execpreset(const char *preset)
{
    rc = sqlite3_exec(db, preset, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
        handleError("Error preset");
    return;
}

void Sqlite::beginTransaction()
{
    char *err = nullptr;
    if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::cerr << "BEGIN failed: " << err << "\n";
        sqlite3_free(err);
    }
}

void Sqlite::commitTransaction()
{
    char *err = nullptr;
    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::cerr << "COMMIT failed: " << err << "\n";
        sqlite3_free(err);
    }
}

void Sqlite::rollbackTransaction()
{
    char *err = nullptr;
    if (sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        std::cerr << "ROLLBACK failed: " << err << "\n";
        sqlite3_free(err);
    }
}

void Sqlite::getCountPair(const char *sql, std::vector<std::pair<std::string, int>> &out)
{
    out.clear();
    Stmt st{nullptr};

    if (sqlite3_prepare_v2(db, sql, -1, &st.ptr, nullptr) != SQLITE_OK)
        return;

    while (sqlite3_step(st.ptr) == SQLITE_ROW)
    {

        const void *txt = sqlite3_column_text(st.ptr, 0);
        std::string author;

        if (txt != nullptr)
        {
            author = reinterpret_cast<const char *>(txt);
        }
        else
        {
            author = "";
        }

        int count = sqlite3_column_int(st.ptr, 1);
        out.emplace_back(author, count);
    }
}

void Sqlite::open(const char *file)
{

    rc = sqlite3_open(file, &db);
    if (rc != SQLITE_OK)
    {
        handleError("Error Opening Database");
        exit(1);
    }

    // session config
    execpreset("PRAGMA journal_mode=WAL;");
    execpreset("PRAGMA synchronous=NORMAL;");
    execpreset("PRAGMA foreign_keys = ON;");

    // version

    version = genericQuery<int>("PRAGMA user_version;");

    if (version < 1)
    {
        execpreset(CT_CHANNELS);
        execpreset(CT_VIDEOS);
        execpreset(CT_SETTING);
        execpreset(IDX1);
        execpreset("INSERT OR IGNORE INTO Setting (id, Keep_Feed) VALUES (1, 30);");
        execpreset("PRAGMA user_version = 1;");
        version = 1;
    }
}

void Sqlite::saveSettings(int keepFeed)
{
    const char *query =
        "INSERT INTO Setting (id, Keep_Feed, Last_updated) "
        "VALUES (1, CASE WHEN ?1 = -1 THEN 30 ELSE ?1 END, strftime('%s', 'now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        "Keep_Feed = CASE WHEN ?1 = -1 THEN Keep_Feed ELSE ?1 END, "
        "Last_updated = strftime('%s', 'now');";

    Stmt st{nullptr};

    if (sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr) != SQLITE_OK)
    {
        handleError("Error prepare saveSetting:");
        return;
    }

    sqlite3_bind_int(st.ptr, 1, keepFeed);

    rc = sqlite3_step(st.ptr);

    if (rc != SQLITE_DONE)
        handleError("Error saveSetting:");
}

int Sqlite::updateChannel(const Channel &ch)
{
    const char *query = "UPDATE Channels SET Name =? WHERE ID = ?;";
    Stmt st{nullptr};

    rc = sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Errore prepare updateChannel ");
        return 0;
    }

    sqlite3_bind_text(st.ptr, 1, ch.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.ptr, 2, ch.id.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(st.ptr);
    if (rc != SQLITE_DONE)
    {
        handleError("Errore updateChannel ");
        return 0;
    }

    return 1;
}

int Sqlite::insertChannel(const std::string &chid)
{

    if (chid.empty())
        return 0;

    int counter = 0;

    const char *query = "INSERT INTO Channels (Id) VALUES (?) ON CONFLICT(Id) DO NOTHING;";
    Stmt st{nullptr};

    rc = sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Errore prepare insertChannel ");
        return 0;
    }

    sqlite3_bind_text(st.ptr, 1, chid.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(st.ptr);
    if (rc == SQLITE_DONE)
        counter = sqlite3_changes(db); // 1 = insert, 0 = already present
    else
        handleError("Errore insertChannel ");

    return counter;
}

int Sqlite::removeChannel(const std::string &name)
{
    beginTransaction();

    const char *query = "DELETE FROM Channels WHERE NAME = ?;";
    Stmt st{nullptr};

    rc = sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Error prepare removeChannel ");
        return 0;
    }

    sqlite3_bind_text(st.ptr, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(st.ptr);
    if (rc != SQLITE_DONE)
    {
        handleError("Error removeChannel ");
        return 0;
    }

    if (sqlite3_changes(db) > 0)
    {
        saveSettings(-1);
        commitTransaction();
        return 1;
    }
    commitTransaction();
    return 0;
}

bool Sqlite::existsChannel(const std::string &name)
{
    const char *query = "SELECT Name FROM Channels WHERE Name = ?;";
    Stmt st{nullptr};

    rc = sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr);

    if (rc != SQLITE_OK)
    {
        handleError("Error prepare: ");
        return false;
    }

    sqlite3_bind_text(st.ptr, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    bool found = (sqlite3_step(st.ptr) == SQLITE_ROW);

    return found;
}

void Sqlite::extractChannels(std::vector<Channel> &vec)
{
    vec.clear();
    const char *query = "SELECT Channels.Id, Channels.Name FROM Channels ORDER BY Channels.Name COLLATE NOCASE;";
    Stmt st{nullptr};

    rc = sqlite3_prepare_v2(db, query, -1, &st.ptr, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Error prepare extractChannels:");
        return;
    }

    while (sqlite3_step(st.ptr) == SQLITE_ROW)
    {
        const void *id = sqlite3_column_text(st.ptr, 0);
        const void *name = sqlite3_column_text(st.ptr, 1);

        Channel ch{};
        ch.id = id ? std::string(reinterpret_cast<const char *>(id)) : "";
        ch.name = name ? std::string(reinterpret_cast<const char *>(name)) : "";
        vec.emplace_back(ch);
    }
    return;
}

void Sqlite::extractVideos(std::vector<Video> &vec, uint32_t lim, const std::string filterChannel, uint32_t lastSecond)
{
    // base query
    std::string query =
        "SELECT Id, Timestamp, Title, Author, Views, Stars, Short";

    if (f == PERCENT)
    {
        query += ", CASE WHEN Views = 0 THEN 0 "
                 "       ELSE (Stars * 100.0 / Views) "
                 "  END AS Percent";
    }

    query += " FROM Videos WHERE 1=1 ";

    if (!filterChannel.empty())
    {
        query += "AND Author = ? ";
    }

    if (tf != NONE)
    {
        switch (tf)
        {
        case BEFORE:
            query += "AND Timestamp < ? ";
            break;

        case AFTER:
            query += "AND Timestamp > ? ";
            break;

        case EQUAL:
        case RANGE:
            query += "AND Timestamp BETWEEN ? AND ? ";
            break;
        }
    }

    if (lastSecond != 0)
    {
        query += "AND Timestamp > strftime('%s','now') - ? ";
    }

    if (vf == ONLY_NORMAL)
        query += "AND Short = 0 ";
    if (vf == ONLY_SHORT)
        query += "AND Short = 1 ";

    std::string orderBy;
    switch (f)
    {
    case TIMESTAMP:
        orderBy = "Timestamp";
        break;
    case AUTHOR:
        orderBy = "Author";
        break;
    case TITLE:
        orderBy = "Title";
        break;
    case VIEWS:
        orderBy = "Views";
        break;
    case STARS:
        orderBy = "Stars";
        break;
    case PERCENT:
        orderBy = "Percent";
        break;
    default:
        orderBy = "Timestamp";
    }

    query += " ORDER BY " + orderBy + (d == DESC ? " DESC" : " ASC");

    query += " LIMIT ?;";

    Stmt st{nullptr};

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &st.ptr, nullptr) != SQLITE_OK)
    {
        handleError("Error prepare extractVideos ");
        return;
    }
    int bindIdx = 1;

    if (!filterChannel.empty())
    {
        sqlite3_bind_text(st.ptr, bindIdx++, filterChannel.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (tf != NONE)
    {
        switch (tf)
        {
        case NONE:
            break;

        case BEFORE:
        case AFTER:
            sqlite3_bind_int64(st.ptr, bindIdx++, static_cast<sqlite3_int64>(tpa[0]));
            break;

        case EQUAL:
        case RANGE:
            sqlite3_bind_int64(st.ptr, bindIdx++, static_cast<sqlite3_int64>(tpa[0]));
            sqlite3_bind_int64(st.ptr, bindIdx++, static_cast<sqlite3_int64>(tpa[1]));
            break;
        }
    }

    if (lastSecond != 0)
    {
        sqlite3_bind_int64(st.ptr, bindIdx++, lastSecond);
    }

    sqlite3_bind_int64(st.ptr, bindIdx, lim);

    vec.clear();
    while (sqlite3_step(st.ptr) == SQLITE_ROW)
    {
        Video v{};
        readVideo(v, st);
        vec.emplace_back(std::move(v));
    }
}

void Sqlite::trimAllAuthor()
{
    int limit = genericQuery<int>("SELECT Keep_Feed FROM Setting WHERE id = 1;");

    std::vector<std::string> authors;
    const char *sqlAuthors = "SELECT DISTINCT Name FROM Channels;";

    sqlite3_stmt *stmt1;

    rc = sqlite3_prepare_v2(db, sqlAuthors, -1, &stmt1, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Error prepare trimAllAuthor:");
        return;
    }
    while (sqlite3_step(stmt1) == SQLITE_ROW)
    {
        const unsigned char *namep = sqlite3_column_text(stmt1, 0);

        if (namep)
        {

            authors.emplace_back(reinterpret_cast<const char *>(namep));
        }
    }
    sqlite3_finalize(stmt1);

    beginTransaction();

    const char *sqlTrim =
        "DELETE FROM Videos WHERE Author = ? AND Timestamp < ("
        "  SELECT Timestamp FROM Videos WHERE Author = ? "
        "  ORDER BY Timestamp DESC LIMIT 1 OFFSET ?"
        ");";

    sqlite3_stmt *stmt2;
    if (sqlite3_prepare_v2(db, sqlTrim, -1, &stmt2, nullptr) == SQLITE_OK)
    {
        for (const auto &author : authors)
        {
            sqlite3_bind_text(stmt2, 1, author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt2, 2, author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt2, 3, limit - 1); // OFFSET 29 = 30°

            sqlite3_step(stmt2);
            sqlite3_reset(stmt2); // Reset
        }
        sqlite3_finalize(stmt2);
    }

    commitTransaction();
}

int Sqlite::insertVideosBatch(const std::vector<Video> &videos)
{
    beginTransaction();
    int counter = 0;

    const char *insertQuery =
        "INSERT INTO Videos (Id, Timestamp, Title, Author, Views, Stars, Short) "
        "VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(Id) DO NOTHING "
        "RETURNING 1;";

    sqlite3_stmt *insertStmt = nullptr;
    rc = sqlite3_prepare_v2(db, insertQuery, -1, &insertStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Error prepare insert");
        return 0;
    }

    const char *updateQuery =
        "UPDATE Videos SET Timestamp=?, Title=?, Author=?, Views=?, Stars=? "
        "WHERE Id=?;";

    sqlite3_stmt *updateStmt = nullptr;
    rc = sqlite3_prepare_v2(db, updateQuery, -1, &updateStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        handleError("Error prepare update");
        sqlite3_finalize(insertStmt);
        return 0;
    }

    for (const auto &vid : videos)
    {
        // std::cout << "extract: [" << vid.id << "] len=" << strlen(vid.id) << "\n";
        sqlite3_bind_text(insertStmt, 1, vid.id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insertStmt, 2, vid.tp);
        sqlite3_bind_text(insertStmt, 3, vid.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 4, vid.author.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(insertStmt, 5, vid.views);
        sqlite3_bind_int64(insertStmt, 6, vid.stars);
        sqlite3_bind_int(insertStmt, 7, vid.sh ? 1 : 0);

        rc = sqlite3_step(insertStmt);

        bool wasInsert = false;

        if (rc == SQLITE_ROW)
        {
            wasInsert = true;
            counter++;
        }
        else if (rc == SQLITE_DONE)
        {
            wasInsert = false;
        }
        else
        {
            handleError("Error during insert step");
        }

        sqlite3_reset(insertStmt);
        sqlite3_clear_bindings(insertStmt);

        if (!wasInsert)
        {
            sqlite3_bind_int64(updateStmt, 1, vid.tp);
            sqlite3_bind_text(updateStmt, 2, vid.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(updateStmt, 3, vid.author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(updateStmt, 4, vid.views);
            sqlite3_bind_int64(updateStmt, 5, vid.stars);
            sqlite3_bind_text(updateStmt, 6, vid.id, -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(updateStmt);

            if (rc != SQLITE_DONE)
                handleError("Error during update");

            sqlite3_reset(updateStmt);
            sqlite3_clear_bindings(updateStmt);
        }
    }

    sqlite3_finalize(insertStmt);
    sqlite3_finalize(updateStmt);

    // if (counter > 0)
    saveSettings(-1);

    commitTransaction();
    return counter;
}

void Sqlite::getVideoBoundaries(Video &out, bool desc, Field fd)
{
    std::string query =
        "SELECT Id, Timestamp, Title, Author, Views, Stars, Short FROM Videos ORDER BY ";

    if (fd == TIMESTAMP)
        query += "Timestamp ";
    if (fd == STARS)
        query += "Stars ";
    if (fd == VIEWS)
        query += "Views ";

    if (desc)
        query += "ASC";
    else
        query += "DESC";

    query += " LIMIT 1;";

    Stmt st{nullptr};

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &st.ptr, nullptr) != SQLITE_OK)
    {
        handleError("Error prepare getVideoBoundaries: ");
        return;
    }

    if (sqlite3_step(st.ptr) == SQLITE_ROW)
    {
        readVideo(out, st);
        return;
    }

    return;
}

void Sqlite::purge(size_t &dc, size_t &dv)
{
    dc = 0;
    dv = 0;

    beginTransaction();

    // 1. Pulizia Canali
    if (sqlite3_exec(db, "DELETE FROM Channels WHERE Name IS NULL OR Name = '';", nullptr, nullptr, nullptr) == SQLITE_OK)
    {
        dc = (size_t)sqlite3_changes(db); // Prendi il valore SUBITO
    }

    // 2. Pulizia Video
    if (sqlite3_exec(db, "DELETE FROM Videos WHERE Title IS NULL OR Title = '';", nullptr, nullptr, nullptr) == SQLITE_OK)
    {
        dv = (size_t)sqlite3_changes(db); // Prendi il valore SUBITO
    }

    commitTransaction();

    sqlite3_exec(db, "VACUUM;", nullptr, nullptr, nullptr);
}

void Sqlite::stat(int width)
{
    FILE *fi = fopen("local.db", "rb");
    if (!fi)
    {
        std::cout << "Not database found\n";
        exit(1);
    }

    fseek(fi, 0, SEEK_END);
    size_t size = ftell(fi);
    fclose(fi);

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

    int schemaVersion = genericQuery<int>("PRAGMA schema_version;");
    int userVersion = genericQuery<int>("PRAGMA user_version;");
    int freelist = genericQuery<int>("PRAGMA freelist_count;");
    int syncr = genericQuery<int>("PRAGMA synchronous;");
    int tables = genericQuery<int>("SELECT COUNT(*) FROM sqlite_master WHERE type='table'");
    int countShort = genericQuery<int>("SELECT COUNT(*) FROM Videos WHERE Short = 1;");
    std::string formattedTime = genericQuery<std::string>("SELECT strftime('%H:%M %d-%m-%Y', Last_updated, 'unixepoch') FROM Setting WHERE id = 1;");
    std::string result = genericQuery<std::string>("PRAGMA integrity_check;");
    std::string mode = genericQuery<std::string>("PRAGMA journal_mode;");
    int fkeys = genericQuery<int>("PRAGMA foreign_keys;");

    Video v1, v2, v3, v4, v5, v6;
    getVideoBoundaries(v1, true, TIMESTAMP);
    getVideoBoundaries(v2, false, TIMESTAMP);
    getVideoBoundaries(v3, true, VIEWS);
    getVideoBoundaries(v4, false, VIEWS);
    getVideoBoundaries(v5, true, STARS);
    getVideoBoundaries(v6, false, STARS);

    std::cout << "\n---------- YOUTUBE STATISTICS -----------\n\n"
              << "Total Channels: " << totalChannels << "\n"
              << "Total Videos: " << totalVideos << "\n"
              << "Total Videos Shorts: " << countShort << "\n"
              << "Recent Videos (last 24 hours): " << last24h << "\n"
              << "Average gap between videos: " << avgGap / 3600.0 << " hours\n";

    std::vector<std::pair<std::string, int>> lastWeek;
    std::vector<std::pair<std::string, int>> lastWeekC;
    std::vector<Video> vec1;
    std::vector<Video> vec2;
    f = PERCENT;
    extractVideos(vec1, 10, "", 864000);
    extractVideos(vec2, 10, "", 4838400);

    getCountPair("SELECT strftime('%d-%m-%Y', Timestamp, 'unixepoch'), COUNT(*) "
                 "FROM Videos "
                 "WHERE Timestamp >= strftime('%s','now') - 864000 "
                 "GROUP BY 1 ORDER BY 1 DESC LIMIT 10;",
                 lastWeek);
    getCountPair(
        "SELECT Author, COUNT(*) FROM Videos "
        "WHERE Timestamp >= strftime('%s','now') - 864000 "
        "GROUP BY Author ORDER BY COUNT(*) DESC LIMIT 10;",
        lastWeekC);

    std::cout << "\n---- ACTIVITY LAST 10 DAYS (" << last7d << std::left << std::setw(27) << " Videos) ---- "
              << "------- TOP CHANNELS ---------          ------- BEST VIDEOS (s/v) ----- \n\n";

    std::cout << std::fixed << std::setprecision(2);
    for (size_t i = 0; i < lastWeek.size(); i++)
    {

        std::string bar1(lastWeek[i].second, '#');
        std::string bar2(lastWeekC[i].second / 2, '#');
        std::cout << std::setw(10) << lastWeek[i].first
                  << std::setw(46) << (" | " + bar1 + " (" + std::to_string(lastWeek[i].second) + ")");
        if (i < lastWeekC.size())
            std::cout << std::setw(12) << lastWeekC[i].first.substr(0, 11)
                      << std::setw(30) << (" | " + bar2 + " (" + std::to_string(lastWeekC[i].second) + ")");
        if (i < vec1.size())
        {
            printLeftPadded(vec1[i].title, 24);
            std::cout << "  ";
            printLeftPadded(vec1[i].author, 12);
            std::cout << "  " << vec1[i].percent << "%";
        }

        std::cout.put('\n');
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

    std::cout << "\n--- ACTIVITY LAST 2 MONTHS Weekly (" << last2m << std::setw(18) << " Videos) ---- "
              << "------- TOP CHANNELS --------           ------- BEST VIDEOS (s/v) ----- \n\n";
    for (size_t i = 0; i < lastWeek.size(); i++)

    {

        std::string bar1((lastWeek[i].second) / 4, '#');
        std::string bar2(lastWeekC[i].second / 2, '#');
        std::cout << std::setw(10) << lastWeek[i].first
                  << std::setw(32) << (" | " + bar1 + " (" + std::to_string(lastWeek[i].second) + ")");
        if (i < lastWeekC.size())
            std::cout << std::setw(12) << lastWeekC[i].first.substr(0, 11)
                      << std::setw(30) << (" | " + bar2 + " (" + std::to_string(lastWeekC[i].second) + ")");
        if (i < vec2.size())
        {
            printLeftPadded(vec2[i].title, 24);
            std::cout << "  ";
            printLeftPadded(vec2[i].author, 12);
            std::cout << "  " << vec2[i].percent << "%";
        }

        std::cout.put('\n');
    }

    responsiveConsole(" NEWEST AND OLDEST VIDEO ", width);
    v2.printVideo(false);
    v1.printVideo(false);
    responsiveConsole(" FIRST AND LAST VIEWS VIDEO ", width);
    std::cout << std::left << std::setw(13) << v4.views;
    v4.printVideo(false);
    std::cout << std::left << std::setw(13) << v3.views;
    v3.printVideo(false);
    responsiveConsole(" FIRST AND LAST STARS VIDEO ", width);
    std::cout << std::left << std::setw(13) << v6.stars;
    v6.printVideo(false);
    std::cout << std::left << std::setw(13) << v5.stars;
    v5.printVideo(false);

    std::cout << "\n----------- DATABASE STATISTICS ----------------\n\n"
              << "Database size: " << size / 1024 << " kb\n"
              << "Last change: " << formattedTime.c_str() << "\n"
              << "Schema version: " << schemaVersion << "\n"
              << "User version: " << userVersion << "\n"
              << "Integrity: " << result.c_str() << "\n"
              << "Freelist pages: " << freelist << "\n"
              << "Journal mode: " << mode.c_str() << "\n"
              << "Synchronous mode: " << syncr << "\n"
              << "Foreign Keys: " << fkeys << "\n"
              << "Limit Videos Saved per Channel: " << keep << "\n"
              << "Total table: " << tables << "\n\n";
}

void Sqlite::close()
{
    if (!db)
        return;

    sqlite3_stmt *stmtCheck = nullptr;
    while ((stmtCheck = sqlite3_next_stmt(db, nullptr)) != nullptr)
    {
        sqlite3_finalize(stmtCheck);
    }

    rc = sqlite3_close(db);
    if (rc != SQLITE_OK)
    {
        std::cerr << "[Sqlite] Warning: DB not closed: "
                  << sqlite3_errstr(rc) << std::endl;
    }

    db = nullptr;
}
