#include <iostream>
#include <string>
#include <iomanip>
#include <utf8proc.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "const.h"

void responsiveConsole(const std::string &ws, int wc)
{

    int remain = wc - (int)ws.length() - 2;
    int half = remain / 2;
    int modul = remain % 2;

    std::string line = "\n" + std::string(half, '-') + ws + std::string(half + modul, '-') + "\n\n";
    std::cout << line;
}

void printHelp()
{
    std::cout << "  ytf available commands:\n\n"
             "  -a, --add <id>         Add a single YouTube channel ID\n"
             "  -l, --list             Show all channels (ID + name) stored in the database\n"
             "  -r, --remove <name>    Delete a channel by name\n"
             "  -e, --ext              Show extended feeds info\n"
             "  -f, --feed <name>      Show feeds from a single channel by name\n"
             "  -i, --index <number>   Show only url at selected index\n"
             "  -k, --keep <number>    Max videos saved per author in database (min: 30)\n"
             "  -L, --load <file>      Load a list of YouTube channel IDs from a text file\n"
             "  -n, --new              Show feeds published in last 24 hours\n"
             "  -o, --order <f> [d]    Sort by 'd'ate, 'a'uthor, 't'itle, 's'tars, 'v'iews\n"
             "                         Direction 'up'/'down'. Defaults: a,t (up), others (down)\n"
             "  -q, --quiet            Offline mode: list only videos in database\n"
             "  -p, --purge            Remove incomplete entries and vacuum database\n"
             "  -s, --limit <number>   Limit the number of feeds printed (default: 20)\n"
             "  -V, --video-only       Hide short videos (show only long-form)\n"
             "  -S, --short-only       Hide long videos (show only shorts)\n"
             "  -x, --stat             Show feeds and database statistics\n"
             "  -w, --web              Generate a static HTML page with selected feeds\n"
             "  -b, --about            Print credits\n"
             "  -v, --version          Show version\n"
             "  -h, --help             Show this help message\n\n";
    
}

void printLogo()
{

    
    const char *RED = "\033[1;31m";
    const char *YEL = "\033[1;33m";
    const char *GRY = "\033[90m";
    const char *RST = "\033[0m"; // reset

    std::string buffer;
    size_t estimatedSize = 0;
    for (size_t i = 0; i < LOGO_LEN; i++)
        estimatedSize += strlen(LOGO[i]) * 10;

    buffer.reserve(estimatedSize);
    for (size_t i = 0; i < LOGO_LEN; i++)
    {
        const char *row = LOGO[i];
        const char *currentColor = nullptr;

        for (size_t j = 0; row[j] != '\0'; j++)
        {
            char c = row[j];
            const char *neededColor;

            if (c == '1')
                neededColor = RED;
            else if (c == '#')
                neededColor = YEL;
            else
                neededColor = GRY;

            if (neededColor != currentColor)
            {
                buffer += neededColor;
                currentColor = neededColor;
            }

            buffer += c;
        }
    }
    buffer += RST;
    std::cout << buffer;
    std::cout << "\nYtf - A cli feed reader for YouTube\n"
              "----------------------------------------------------\n"
              "Version               1.0.0\n"
              "Build date            2026-03-21\n"
              "Author                Adalberto Serra\n"
              "GitHub                https://github.com/AdaSerra/Ytf\n"
              "License               MIT\n";
}

int getConsoleWidth()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return FALLBACK_WIDTH_CONSOLE;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        return w.ws_col;
    }
    return FALLBACK_WIDTH_CONSOLE;
#endif
}

void printLeftPadded(std::string_view s_arg, size_t width)
{

    /*  if (c == '&')
     {
         size_t semi = input.find(';', i);
         if (semi != std::string_view::npos && (semi - i) < 8)
         {
             std::string_view entity = input.substr(i, semi - i + 1);
             std::string_view decoded;
             if (entity == "&quot;")
                 decoded = "\"";
             else if (entity == "&amp;")
                 decoded = "&";
             else if (entity == "&apos;")
                 decoded = "'";
             else if (entity == "&lt;")
                 decoded = "<";
             else if (entity == "&gt;")
                 decoded = ">";
             else if (entity == "&eacute;")
                 decoded = "é";
             else if (entity == "&agrave;")
                 decoded = "à";

             if (!decoded.empty())
             {
                 std::cout << decoded;
                 visualWidth++;
                 i = semi + 1;
                 continue;
             }
         }
     } */

    if (s_arg.empty())
    {
        std::cout << std::string(width, ' ');
        return;
    }

    // 1. Normalize NFKC
    utf8proc_uint8_t *norm = utf8proc_NFKC((const utf8proc_uint8_t *)s_arg.data());
    if (!norm)
    {
        // Fallback raw
        std::string fallback(s_arg.substr(0, width));
        std::cout << fallback << std::string(width > fallback.length() ? width - fallback.length() : 0, ' ');
        return;
    }

    const utf8proc_uint8_t *p = norm;
    size_t len = strlen((const char *)p);
    size_t i = 0;
    int visualUsed = 0;
    size_t lastValidByte = 0;

    // 2. width and chop
    while (i < len)
    {
        utf8proc_int32_t cp;
        ssize_t consumed = utf8proc_iterate(p + i, len - i, &cp);
        if (consumed <= 0)
            break;

        // this codepoint continue the last ?
        const utf8proc_property_t *prop = utf8proc_get_property(cp);
        int w = utf8proc_charwidth(cp);

        // Variation selector → width = 0
        if (cp == 0xFE0F)
        {
            w = 0;
        }
        // Emoji → SO category
        else if (prop->category == UTF8PROC_CATEGORY_SO)
        {
            // If utf8proc says 1 but the terminal shows it as 2 wide → fix
            if (w == 1)
                w = 2;
        }

        if (visualUsed + w > (int)width)
            break;

        visualUsed += w;
        i += consumed;
        lastValidByte = i;
    }

    // 3.Prints the part of the string that fits within the width
    if (lastValidByte > 0)
    {
        std::cout.write((const char *)p, lastValidByte);
    }

    // 4. right padding
    int spacesNeeded = (int)width - visualUsed;
    if (spacesNeeded > 0)
    {
        for (int k = 0; k < spacesNeeded; ++k)
        {
            std::cout << ' ';
        }
    }

    free(norm);
}

void debugBytes(std::string_view s)
{
    std::cout << "LEN = " << s.size() << "\n";

    for (size_t i = 0; i < s.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(s[i]);

        std::cout << "i=" << std::setw(2) << i
                  << "  HEX=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)c
                  << std::dec << std::setfill(' ')
                  << "  CHAR=";

        if (c >= 32 && c < 127)
            std::cout << "'" << s[i] << "'";
        else
            std::cout << "(non-ASCII)";

        std::cout << "\n";
    }
}

/*  static inline uint32_t utf8ToCodepoint(const char *s, size_t &i, size_t len)
   {
       unsigned char c = s[i];

       if (c < 0x80)
       {
           return s[i++];
       }
       else if ((c & 0xE0) == 0xC0 && i + 1 < len)
       {
           uint32_t cp = ((c & 0x1F) << 6) |
                         (s[i + 1] & 0x3F);
           i += 2;
           return cp;
       }
       else if ((c & 0xF0) == 0xE0 && i + 2 < len)
       {
           uint32_t cp = ((c & 0x0F) << 12) |
                         ((s[i + 1] & 0x3F) << 6) |
                         (s[i + 2] & 0x3F);
           i += 3;
           return cp;
       }
       else if ((c & 0xF8) == 0xF0 && i + 3 < len)
       {
           uint32_t cp = ((c & 0x07) << 18) |
                         ((s[i + 1] & 0x3F) << 12) |
                         ((s[i + 2] & 0x3F) << 6) |
                         (s[i + 3] & 0x3F);
           i += 4;
           return cp;
       }

       // fallback
       return s[i++];
   }

   static inline int charWidth(uint32_t cp)
   {
       if (cp < 32)
           return 0; // Caratteri di controllo

#ifdef _WIN32
       // Range per caratteri "Larghi" (Double Width / CJK / Emoji)
       if ((cp >= 0x1100 && cp <= 0x115F) || // Hangul Jamo
           cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x2E80 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE10 && cp <= 0xFE19) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0x1F300 && cp <= 0x1FAFF))
       {
           return 2;
       }
       // Tutto il resto (inclusa la 'é' -> U+00E9) occupa 1 spazio
       return 1;
#else
       int w = wcwidth(cp);
       return (w < 0) ? 1 : w;
#endif
   }

   bool isNarrowEmoji(utf8proc_int32_t cp)
   {
       switch (cp)
       {
       case 0x1F6E0: // 🛠️ hammer and wrench
           return true;

           // Se in futuro trovi altre emoji "larghe 1", aggiungile qui.
       }
       return false;
   } */