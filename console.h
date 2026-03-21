#pragma once
#include <cstddef>
#include <string_view>
#include <string>

void printHelp();
void printLogo();
int getConsoleWidth();
void responsiveConsole(const std::string &ws, int wc);
void printLeftPadded(std::string_view s_arg, size_t width);
void debugBytes(std::string_view s);
