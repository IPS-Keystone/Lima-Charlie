#include "lc_presence.h"

#include <stdio.h>
#include <string.h>

#define LC_TAG_OPEN "<lc>"
#define LC_TAG_CLOSE "</lc>"
#define LC_TAG_OPEN_LEN (sizeof(LC_TAG_OPEN) - 1)
#define LC_TAG_CLOSE_LEN (sizeof(LC_TAG_CLOSE) - 1)

/* Finds our section in meta. Returns 1 and its bounds, tags included. */
static int find_section(const char* meta, const char** begin, const char** end)
{
    const char* open = meta ? strstr(meta, LC_TAG_OPEN) : NULL;
    if (!open)
        return 0;
    const char* close = strstr(open + LC_TAG_OPEN_LEN, LC_TAG_CLOSE);
    if (!close)
        return 0;
    *begin = open;
    *end   = close + LC_TAG_CLOSE_LEN;
    return 1;
}

size_t lc_presence_format(char* out, size_t cap, int connected, const char* pluginVersion, const char* modVersion)
{
    const int n = _snprintf(out, cap, LC_TAG_OPEN "%d|%s|%s" LC_TAG_CLOSE, connected ? 1 : 0,
                            pluginVersion ? pluginVersion : "", modVersion ? modVersion : "");
    if (n < 0 || (size_t)n >= cap) {
        if (cap)
            out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

int lc_presence_merge(const char* meta, const char* section, char* out, size_t cap)
{
    if (!meta)
        meta = "";
    if (!section)
        section = "";

    const char* begin   = NULL;
    const char* end     = NULL;
    const int   had     = find_section(meta, &begin, &end);
    const size_t before = had ? (size_t)(begin - meta) : strlen(meta);
    const char*  after  = had ? end : "";
    const size_t total  = before + strlen(section) + strlen(after);
    if (total >= cap) {
        if (cap)
            out[0] = '\0';
        return 0;
    }
    memcpy(out, meta, before);
    out[before] = '\0';
    strcat(out, section);
    strcat(out, after);
    return 1;
}

/* Copies one '|'-separated field, keeping only what a version string is made of. Returns where it stopped. */
static const char* copy_version(const char* at, const char* end, char* out, size_t cap)
{
    size_t i = 0;
    while (at < end && *at != '|') {
        const char c    = *at++;
        const int  safe = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_' || c == '+';
        if (safe && i + 1 < cap)
            out[i++] = c;
    }
    out[i] = '\0';
    return at < end ? at + 1 : at;
}

int lc_presence_parse(const char* meta, lc_presence* out)
{
    memset(out, 0, sizeof(*out));
    const char* begin = NULL;
    const char* end   = NULL;
    if (!find_section(meta, &begin, &end))
        return 0;
    begin += LC_TAG_OPEN_LEN;
    end -= LC_TAG_CLOSE_LEN;

    out->connected = begin < end && *begin == '1';
    while (begin < end && *begin != '|')
        ++begin;
    if (begin < end)
        ++begin;
    begin = copy_version(begin, end, out->pluginVersion, sizeof(out->pluginVersion));
    copy_version(begin, end, out->modVersion, sizeof(out->modVersion));
    return 1;
}
