## Ytfeed‑cli

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg) ![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C.svg) ![SQLite](https://img.shields.io/badge/database-SQLite-003B57.svg) ![WinHTTP](https://img.shields.io/badge/network-WinHTTP-0082C9.svg) ![MSXML6](https://img.shields.io/badge/XML-MSXML6-6A5ACD.svg)

A fast, lightweight YouTube RSS fetcher and SQLite indexer for Windows.

### 📌 Overview
Ytfeed‑cli is a minimalistic command‑line tool designed to fetch YouTube channel feeds via RSS, parse them using MSXML6, and store normalized video metadata into a local SQLite database.
It is the console counterpart of a more complete WinRT/WinUI application, sharing the same core logic but optimized for automation, scripting,  and low‑overhead usage.

The tool is built entirely on native Windows APIs:

WinHTTP for HTTPS requests

MSXML6 for XML parsing

SQLite (winsqlite3) for persistent storage

C++20 for performance and portability

Its goal is simple: track multiple YouTube channels, store their latest videos, and present them sorted by publication date.

### ✨ Features
Fetches YouTube RSS feeds directly (no API key required)

Parses XML using MSXML6 with robust error handling

Stores channels and videos in SQLite with conflict‑free inserts

Automatically trims old videos (keeps the latest 30 per channel)

Supports batch insertion for high performance

Validates YouTube channel IDs with strict regex

Validates Windows filenames safely

UTF‑8 → UTF‑16 conversion for full Unicode support

Clean, deterministic output suitable for scripting

### 🛠 Requirements

- Windows 10 or later
- Visual Studio (MSVC)
- SQLite (winsqlite3.lib)
- MSXML6
- WinHTTP

All dependencies are native to Windows.

### 🚀 Usage
Basic syntax
```
ytfeed-cli [options]
 
Option	Description
  -a, -A <id>       Add a single YouTube channel ID
  -c, -C            Show all channels (ID + name) stored in the database
  -d, -D <name>     Delete a channel by name
  -f, -F <name>     Show feeds from a single channel by name
  -l, -L <file>     Load a list of YouTube channel IDs from a text file
  -s, -S <number>   Limit the number of feeds printed (default: 20)
  -h, -H            Show this help message
  
```

📄 Input file format (-L)

A plain text file containing one YouTube channel ID per line:
```
UC_x5XG1OV2P6uZZ5FSM9Ttw
UC4R8DWoMoI7CAwX8_LjQHig
UCYO_jab_esuFRV4b17AJtAw
```
Invalid IDs are rejected with a warning.

### 🔍 Example commands
Load channels from file:
```
ytfeed-cli -L channels.txt
```
Add a single channel:
```
ytfeed-cli -a UC_x5XG1OV2P6uZZ5FSM9Ttw
```
Show the latest 10 videos:
```
ytfeed-cli -s 10
```
🧠 How it works (internal pipeline)

- Parse arguments  
- Validate filenames, numeric limits, and channel IDs.
- Load channels from file (-L) or single ID (-a).
- Fetch RSS feeds  
- Using WinHTTP with a lightweight user agent.
- Parse XML  
- Extract: channel name, video title, video ID, publication timestamp
- Insert into SQLite  
  Using prepared statements and batch transactions.
- Trim old videos  
  Keep only the latest 30 per channel.
- Sort and display  
  Videos are sorted by timestamp (newest first).

🧩 Database schema
```
  Channels Table
| Column | Type    |  Description       |
|--------|---------|--------------------|
| Id     | TEXT PK | YouTube Channel Id |
| Name   | TEXT    | Channel name       | (fetched from feed)

 Videos Table
| Column    | Type    |  Description     |
|-----------|---------|------------------|
| Id        | TEXT PK | YouTube Video Id |  
| Title     | TEXT    | Video title      | 
| Author    | TEXT    | Channel name     | FK REFERENCES Channels(Id)
| Timestamp | INTEGER | Unix timestamp   |


```
### ⚙ Build instructions
```
MSVC (Developer Command Prompt)
cl ytfeed-cli.cpp /O2 /EHsc /std:c++20 ```