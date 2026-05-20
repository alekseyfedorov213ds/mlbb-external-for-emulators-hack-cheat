#pragma once
#include <cstdio>
#include <string>

namespace phlog {
inline void info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[INFO] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
inline void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[ERR ] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}
}
