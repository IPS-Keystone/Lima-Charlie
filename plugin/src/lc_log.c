#include "lc_log.h"

#include <stdarg.h>
#include <stdio.h>

static lc_log_sink g_sink;

void lc_log_set_sink(lc_log_sink sink)
{
    g_sink = sink;
}

void lc_logf(enum lc_log_level level, const char* fmt, ...)
{
    const lc_log_sink sink = g_sink;
    if (!sink)
        return;
    char    buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    sink(level, buf);
}
