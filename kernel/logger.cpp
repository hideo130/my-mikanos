#include "logger.hpp"

#include <cstddef>
#include <cstdio>

#include "console.hpp"

namespace
{
    LogLevel log_level = kError;
}

extern Console *console;

void SetLogLevel(LogLevel level)
{
    log_level = level;
}

int Log(LogLevel level, const char *format, ...)
{
    // console->PutString("hoge\n");

    if (level > log_level)
    {
        return 0;
    }

    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}