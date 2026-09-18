#pragma once

enum lc_log_level {
    LC_LOG_DEBUG,
    LC_LOG_INFO,
    LC_LOG_WARNING,
    LC_LOG_ERROR
};

typedef void (*lc_log_sink)(enum lc_log_level level, const char* message);

void lc_log_set_sink(lc_log_sink sink);

/* Thread-safe as long as the sink is. */
void lc_logf(enum lc_log_level level, const char* fmt, ...);
