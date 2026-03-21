#include <stdio.h>
#include <string.h>
#include <cstddef>     
#include <cstdint>      
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <ctime>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#define CURL_STATICLIB
#include <curl/curl.h>
#include "const.h"
#include "console.h"
#include "sqll.h"
#include "types.h"
#include "util.h"



#pragma comment(lib, "libcurl.lib")

int main(int argc, char *argv[])
 {

    uint32_t limit = DEFAULT_LIMIT_CONSOLE;
    uint32_t keep = DEFAULT_LIMIT_FEED;
    int width = DEFAULT_WIDTH_CONSOLE;

    char *filename = nullptr;
    std::string readBuffer = "";
    std::string channel = "";
    std::vector<std::string> channels;
    std::vector<Video> videos;
    std::vector<Channel> chns;

    // boolean flags
    bool singleChannel = false;
    bool web = false;
    bool news = false;
    bool quiet = false;
    bool singleVideo = false;
    bool extendFormat = false;

    // counter
    size_t newchan = 0;
    size_t newvideo = 0;
    size_t indexVideo;

    // console setup
    width = getConsoleWidth();
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(std::locale("en_US.UTF-8"));
#else
    std::locale::global(std::locale::classic()); 
#endif
    
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // database
    Sqlite db("local.db");

    // argument parsing
    for (int i = 1; i < argc; i++)
    {

        // exit after
        // --- STATISTICS ---
        if (strcmp(argv[i], "--stat") == 0 || strcmp(argv[i], "-x") == 0)
        {
            db.stat(width);
            return 0;
        }

        // --- HELP -h ---
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            printHelp();
            return 0;
        }

        //  --- Version 
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            std::cout << VERS;
            return 0;
        }
        //  --- about 
        if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--about") == 0)
        {
            printLogo();
            return 0;
        }

        // --- SHOW ALL CHANNELS -l ---
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0)
        {
            db.extractChannels(chns);
            for (const auto &c : chns)
                c.printChannel();
           return 0;
        }
        // ---PURGE---
        if (strcmp(argv[i], "--purge") == 0 || strcmp(argv[i], "-p") == 0)
        {
            db.purge(newchan, newvideo);

            if (newchan > 0 || newvideo > 0)
            {
                std::cout << "[Purge] Database cleaned:\n";
                if (newchan > 0)
                    std::cout << "  - Removed " << newchan << " invalid channels.\n";
                if (newvideo > 0)
                    std::cout << "  - Removed " << newvideo << " corrupted video records.\n";
                std::cout << "  - Database optimized (VACUUM).\n";
                return 0;
            }
            else
            {
                std::cout << "[Purge] Database is already clean. No records removed.\n";
            }
            return 0;
        }
        // --- DELETE CHANNEL -r ---
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--remove") == 0)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "[Remove] Error: missing channel name\n";
               return 1;
            }

            std::string wid = argv[i + 1];
            int del = db.removeChannel(wid);

            if (del == 1)
                std::cout << "[Remove] Channel successfully deleted: " << wid << "\n";
            else
                std::cout << "[Remove] Channel not present: " << wid << "\n";

            return 0;
        }

        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0)
        {
            if (i + 1 < argc)
            {
                int tmp;
                if (parse_int(argv[++i], tmp))
                { // ++i consuma il valore
                    indexVideo = (tmp >= 0) ? (size_t)tmp : 0;
                    singleVideo = true;
                }
            }
            else
            {
                std::cerr << "[Index} Error: -i requires a number\n";
                return 1;
            }
            continue;
        }
        // --- LIMIT VIDEO SHOW IN CONSOLE -s ---
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--limit") == 0)
        {
            int tmp;
            if (i + 1 < argc && parse_int(argv[i + 1], tmp))
            {
                limit = tmp;
                i++;
            }
            else
            {
                std::cerr << "[Limit] Error: -s requires a number\n";
                return 1;
            }
            continue;
        }
        // --- LIMIT VIDEO SAVED IN DB ---
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0)
        {
            if (i + 1 < argc)
            {
                int tmp;
                if (parse_int(argv[i + 1], tmp))
                {
                    keep = (uint32_t)tmp < DEFAULT_LIMIT_FEED ? DEFAULT_LIMIT_FEED : (uint32_t)tmp;
                    db.saveSettings(keep);
                    i++;
                }
                else
                {
                    std::cerr << "[Keep] Error: -K requires a number\n";
                    return 1;
                }
            }
            continue;
        }

        // --- LOAD FILE -L ---
        if (strcmp(argv[i], "-L") == 0 || strcmp(argv[i], "--load") == 0)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "[System] Error: missing file name\n";
                return 1;
            }

            filename = argv[i + 1];

            if (!isValidFilename(filename))
            {
                std::cerr << "[System] Error: file name not valid\n";
                return 1;
            }

            readFile(channels, filename);

            if (!channels.empty())
            {
                db.beginTransaction();
                for (const auto &c : channels)
                    newchan += db.insertChannel(c);
                db.commitTransaction();
                if (newchan > 0)
                {
                    std::cout << "[Add] New channels added: " << newchan;
                    db.saveSettings(-1);
                    return 0;
                }
            }
            else
            {
                std::cout << "[Add] No one channel added\n";
                return 0;
            }
        }

        // --- ADD CHANNEL -A ---
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--add") == 0)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "[Add] Error: missing channel Id\n";
                return 1;
            }

            std::string wid = argv[i + 1];

            if (!isValidChannelID(wid))
            {
                std::cerr << "[Add] Error: channel Id not valid " << wid << "\n";
               return 1;
            }

            newchan = db.insertChannel(wid);

            if (newchan > 0)
            {
                std::cout << "[Add] New channel added with Id: " << wid;
                db.saveSettings(-1);
            }
            else 
            {
                std::cout <<"[Add] Channel already present\n";
            }
            return 0;
        }

        // --- SHOW SINGLE CHANNEL -F ---
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--feed") == 0)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "[Feed] Error: missing channel name\n";
                return 1;
            }

            channel = argv[i + 1];

            singleChannel = db.existsChannel(channel);

            if (!singleChannel)
            {
                std::cout << "[Feed] Channel not present: " << channel << "\n";
                return 0;
            }
        }
        //  ---- ORDER 
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--order") == 0)
        {
            if (i + 1 < argc)
            {
                char field = argv[++i][0];

                switch (field)
                {
                case 't':
                    db.f = TITLE;
                    db.d = ASC;
                    break;
                case 'v':
                    db.f = VIEWS;
                    db.d = DESC;
                    break;
                case 's':
                    db.f = STARS;
                    db.d = DESC;
                    break;
                case 'a':
                    db.f = AUTHOR;
                    db.d = ASC;
                    break;
                case 'd':
                    db.f = TIMESTAMP;
                    db.d = DESC;
                    break;
                default:
                    std::cerr << "[Order] Unknown order field: " << field << "\n";
                    continue;
                }

                if (i + 1 < argc)
                {
                    std::string nextArg = argv[i + 1];
                    if (nextArg == "up")
                    {
                        db.d = ASC;
                        i++;
                    }
                    else if (nextArg == "down")
                    {
                        db.d = DESC;
                        i++;
                    }
                }
                continue;
            }
        }

        // config
        // --- NEW VIDEOS FLAG -n / --new ---
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--new") == 0)
        {
            news = true;
            continue;
        }
        // ---QUIET FLAG -q / --quiet ---
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
        {
            quiet = true;
            continue;
        }
        // ---VIDEO LONG FLAG -V / --video-only ---
        if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--video-only") == 0)
        {
            db.vf = ONLY_NORMAL;
            continue;
        }
        // ---SHORT FLAG -Z / --short-only ---
        if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--short-only") == 0)
        {
            db.vf = ONLY_SHORT;
            continue;
        }
        // --- WEB MODE -W ---
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--web") == 0)
        {
            web = true;
            continue;
        }
        // ---Extended format
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--ext") == 0)
        {
            extendFormat = true;
            continue;
        }
    }

    db.extractChannels(chns); 

    videos.reserve(chns.size() * 15);

    // flag quiet/http request
    if (!quiet)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        CURL *curl;
        curl = curl_easy_init();

        if (curl)
        {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
            for (auto &ch : chns)
            {
                readBuffer.clear();
                char url[512];
                snprintf(url, sizeof(url), "%s%s", YTURL_FEED, ch.id.c_str());
                curl_easy_setopt(curl, CURLOPT_URL, url);

                CURLcode res = curl_easy_perform(curl);

                if (res == CURLE_OK)
                {
                    long http_code = 0;
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

                    if (http_code == 200)
                    {
                        parsingXml(readBuffer, videos, ch, db);
                    }
                    else
                    {
                        std::cerr << "[Curl] Error HTTP for channel " << ch.id << ": " << http_code << std::endl;
                        continue;
                    }
                }
                else
                {
                    std::cerr << "[Curl] Network error: " << curl_easy_strerror(res) << std::endl;
                    return 1;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }

            newvideo = db.insertVideosBatch(videos);

            videos.clear();
        }
        curl_global_cleanup();
    }

    // flag new/recent video
    if (news)
    {
        db.extractVideosLast24h(videos);
    }
    else if (singleChannel)
    {
        db.extractVideos(videos, limit, channel);
    }
    else
    {
        db.extractVideos(videos, limit);
    }

    // flag single Video
    if (singleVideo)
    {
        std::cout << "https://www.youtube.com/watch?v=" << videos[indexVideo < videos.size() ? indexVideo : 0].id << std::endl;
        return 0;
    }

    // flag web page
    if (web)
    {
        generateHTML(videos, newchan, newvideo, limit);
        return 0;
    }

    responsiveConsole(" YOUTUBE FEED UPDATE ", width);
    if (!quiet)
        std::cout << "New Video(s): " << newvideo << "\n";
    if (newchan > 0)
        std::cout << "New Channel(s): " << newchan << "\n";

    for (size_t i = 0; i < videos.size(); i++)
    {
        if (extendFormat)
            videos[i].printVideo(true, i);
        else
            videos[i].printVideo(false);
    }

    std::cout << "\n";

    if (!quiet)
        db.trimAllAuthor();

    return 0;
}