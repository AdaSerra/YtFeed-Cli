#pragma once
#include <string.h>
#include <stdint.h>
#include "types.h"

inline constexpr const char* YTURL_FULL = "https://www.youtube.com/watch?v=";
inline constexpr const char* YTURL_SHORT = "https://youtu.be/";
inline constexpr const char* YTURL_FEED = "https://www.youtube.com/feeds/videos.xml?channel_id=";
inline constexpr const char * YTF_VERSION = "1.2.2";
inline constexpr const char * YTF_BUILD_DATE = "04-09-2026";
inline constexpr const char * DB_NAME = "local.db";

constexpr uint32_t DEFAULT_LIMIT_CONSOLE = 20;
constexpr uint32_t DEFAULT_LIMIT_FEED = 30;
constexpr int DEFAULT_WIDTH_CONSOLE = 150;
constexpr int FALLBACK_WIDTH_CONSOLE = 80;
constexpr size_t ENTRY_BUFFER_SIZE = 65536;

#if defined(_WIN32) || defined(_WIN64)
inline constexpr const char * USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";
#elif defined(__APPLE__)
inline constexpr const char* USER_AGENT = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";
#elif defined(__linux__) || defined (__unix__)
inline constexpr const char* USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36";
#else
inline constexpr const char* USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:149.0) Gecko/20100101 Firefox/149.0";
#endif
//static const std::regex CHANNEL_ID_REGEX("^UC[A-Za-z0-9_-]{22}$");
// static const std::regex specials("(\xe2\x80\x98|\xe2\x80\x99|\xe2\x80\x9c|\xe2\x80\x9d)");

              