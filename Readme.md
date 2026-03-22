## Ytf

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C.svg) ![SQLite](https://img.shields.io/badge/SQLite-embedded-003B57.svg) ![curl](https://img.shields.io/badge/cURL-required-073551.svg) ![utf8proc](https://img.shields.io/badge/Unicode-utf8proc-6A5ACD.svg)


![logo](logo3.png "Ytf")

A fast, lightweight YouTube RSS fetcher and SQLite indexer.

### 📌 Overview

Ytf is a minimalistic CLI tool that:

Fetches YouTube RSS feeds (no API key required)

Parses XML manually (no DOM, no external XML libs)

Stores channels & videos in SQLite

Normalizes Unicode with utf8proc

Outputs clean, deterministic console results

Everything except curl is embedded and statically linked.

### ✨ Features

Direct RSS fetching via HTTPS

Manual XML parsing with robust error handling

SQLite storage with conflict‑free inserts

Automatic trimming (keeps latest N videos per channel)

UTF‑8 normalization (NFC)

Filename validation

Extract only URL at a specific index

Clean output for scripting

Statistics and activity summaries

### 🔍 Querying & Filtering

Sort by date, author, title, stars, views

Filter by channel

Show extended info

Limit output

Show only new videos (last 24h)

Show only shorts / only long‑form

Generate static HTML page


### 📊 Statistics & Insights

The tool can also compute lightweight statistics directly from the database:

Most Channel activity in past days
Best Videos

Publication frequency

Last update timestamps

Oldest/newest indexed entries

Quick summaries for scripting or monitoring setups

### 🛠 Requirements

Mandatory
A C++20 compiler (MSVC, GCC, Clang)

curl

Linux/macOS: system package

Windows: libcurl.dll must be present next to the executable

Embedded (no external install needed)
SQLite (compiled from sqlite3.c)

utf8proc (compiled from utf8proc.c)


### 🚀 Usage
Basic syntax

```
ytf [options]
```
```
ytf -L channels.txt
ytf -a UC_x5XG1OV2P6uZZ5FSM9Ttw
ytf -s 10 -o v
ytf -w
```

```
ytf -h
 
Option	Description
  -a, --add <id>          Add a single YouTube channel ID
  -l, --list              Show all channels (ID + name) stored in the database"
  -r, --remove <name>     Delete a channel by name
  -e, --ext               Show extended feeds info
  -f, --feed <name>       Show feeds from a single channel by name
  -i, --index <number>    Show only url at selected index
  -j, --json              Print output in json format
  -k, --keep <number>     Max videos saved per author in database (min: 30)
  -L, --load <file>       Load a list of YouTube channel IDs from a text file
  -n, --new               Show feeds published in last 24 hours
  -o, --order <s> [d]     Sort by 'd'ate, 'a'uthor, 't'itle, 's'tars, 'v'iews
                          Direction 'up'/'down'. Defaults: a,t (up), others (down)
  -q, --quiet             Offline mode: list only videos in database
  -p, --purge             Remove incomplete entries and vacuum database
  -s, --limit <number>    Limit the number of feeds printed (default: 20)
  -t, --time <f> [<date>] Date filter: 'e'qual (whole day), 'a'fter, 'b'efore, 'r'ange (requires two date)
                          Date format DD-MM-YYYY\n"
  -V, --video-only        Hide short videos (show only long-form)
  -S, --short-only        Hide long videos (show only shorts)
  -x, --stat              Show feeds and database statistics
  -w, --web               Generate a static HTML page with selected feeds
  -b, --about             Print credits
  -v, --version           Show version
  -h, --help              Show this help message
  
```

No one arguments, default output:
```
ytf
---------------------------------------------------------------- YOUTUBE FEED UPDATE-------------------------------------------------------------

New Video(s): 3
19:46:00 20-03-2026  Linus Tech Tips        Testing TikTok hacks...                                     https://youtu.be/8id_d_Kz2Ic
16:01:20 20-03-2026  Smosh                  We Made A Reality Dating Show | Bit City                    https://youtu.be/mpvbhS89UpY
16:00:26 20-03-2026  National Geographic    As spring rolls around, beehives buzz with life and         https://youtu.be/Fo6TlVlLi50
16:00:03 20-03-2026  MrBeast                I Hid $1,000,000 In This Vault                              https://youtu.be/jCE1Ol3PYDs
.....

```

Show some database statistics:
```
ytf -x

---------- YOUTUBE STATISTICS -----------

Total Channels: 15
Total Videos: 225
Total Videos Shorts: 67
Recent Videos (last 24 hours): 5
Average gap between videos: 228.667 hours

---- ACTIVITY LAST 10 DAYS (32 Videos) ----              ------- TOP CHANNELS ---------          ------- BEST VIDEOS (s/v) -----

20-03-2026 | ##### (5)                                  National Ge  | ###### (13)                We Made A Reality Dating  Smosh         11.39%
19-03-2026 | ######## (8)                               Linus Tech   | ##### (10)                 Martian Soil Is Deadly.   PBS Space Ti  9.48%
18-03-2026 | ### (3)                                    Smosh        | ## (4)                     I Fixed YouTube !         PewDiePie     8.60%
17-03-2026 | ##### (5)                                  TED-Ed       | # (3)                      He 3D Printed a case ins  Linus Tech T  7.86%
......
```

### 📄 Input file format (-L)

A plain text file containing one YouTube channel ID per line:
```
UC_x5XG1OV2P6uZZ5FSM9Ttw
UC4R8DWoMoI7CAwX8_LjQHig
UCYO_jab_esuFRV4b17AJtAw
```
Invalid IDs are rejected with a warning.


### 🧠 How it works (internal pipeline)

- Parse arguments  
- Validate filenames, numeric limits, and channel IDs
- Load channels from file (-L) or single ID (-a)
- Fetch RSS feeds  
- Using WinHTTP with a lightweight user agent
- Parse XML  
- Extract: channel name, video title, video ID, publication timestamp
- Insert into SQLite  
  Using prepared statements and batch transactions.
- Trim old videos  
  Keep only the at least latest 30 per channel
- Sort and display  
  Videos are sorted by timestamp (newest first)

### 🧩 Database schema
```
  Channels Table
| Column | Type    |  Description       |
|--------|---------|--------------------|
| Id     | TEXT PK | YouTube Channel Id |
| Name   | TEXT    | Channel name       | (fetched from feed)

 Videos Table
| Column    | Type    |  Description             |
|-----------|---------|------------------------- |
| Id        | TEXT PK | YouTube Video Id         |  
| Title     | TEXT    | Video title              |
| Views     | INTEGER | Video views              |
| Stars     | INTEGER | Video stars (likes)      |   
| Author    | TEXT    | Channel name             | FK REFERENCES Channels(Name)
| Short     | INTEGER | Short Video (1) or not(0)|
| Timestamp | INTEGER | Unix timestamp           |


```
### ⚙ Build instructions

####  Option 1 — CMake (recommended)
The project includes a full CMake setup with automatic curl fetch:
```
bash

cmake -B build
cmake --build build --config Release
```
Notes
On Windows, curl is fetched via FetchContent and built dynamically (libcurl.dll).

On Linux/macOS, system curl is used if available.

SQLite and utf8proc are always compiled statically.

####  Option 2 — PowerShell build script
For quick builds without CMake:

```
pwsh 

build.ps1
```
Windows
Uses MSVC (cl.exe)

Compiles SQLite and utf8proc statically

Requires libcurl.dll in the same folder as the final executable

Runtime: /MD (dynamic), compatible with curl

Linux/macOS
Uses clang++ or g++

Links against system curl (-lcurl)

Produces a single ytf binary

### 📦 Relaese Notes

Png logo included

Windows release includes libcurl.dll

Database is created automatically on first run

### 📜 License

MIT License — see LICENSE for details.
