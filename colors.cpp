#include "Headers.hpp"
#include <stdarg.h>
#include <stdio.h>

// External function from main.cpp
extern void AddLog(const std::string& text);

void SetConsoleColor(int color) {
    // Not needed for ImGui
}

void ResetConsoleColor() {
    // Not needed for ImGui
}

void printf_red(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}

void printf_green(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}

void printf_yellow(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}

void printf_blue(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}

void printf_cyan(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}

void printf_magenta(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    AddLog(std::string(buffer));
}