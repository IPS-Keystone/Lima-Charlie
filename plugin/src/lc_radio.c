#include "lc_radio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LC_RADIO_FIELDS 11

size_t lc_radio_format_announcement(char* out, size_t cap, const char* token, int playerId, int on, int frequency, float range, const float pos[3], const char* key)
{
    /* The key is the last field, so only a separator inside it could break parsing. */
    char safeKey[LC_RADIO_KEY_CAP];
    size_t i = 0;
    for (; key && key[i] && i + 1 < sizeof(safeKey); ++i)
        safeKey[i] = key[i] == '|' ? '_' : key[i];
    safeKey[i] = '\0';

    const int n = _snprintf(out, cap, LC_RADIO_COMMAND_PREFIX "%s|%d|%d|%d|%.0f|%.1f|%.1f|%.1f|%s", token, playerId, on ? 1 : 0, frequency, range, pos[0], pos[1], pos[2], safeKey);
    if (n <= 0 || (size_t)n >= cap)
        return 0;
    return (size_t)n;
}

int lc_radio_parse_announcement(const char* command, lc_radio_announcement* out)
{
    const size_t prefixLen = sizeof(LC_RADIO_COMMAND_PREFIX) - 1;
    if (!command || strncmp(command, LC_RADIO_COMMAND_PREFIX, prefixLen) != 0)
        return 0;

    char buf[512];
    strncpy(buf, command + prefixLen, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Split into the 9 fields after the prefix; the key keeps the remainder. */
    char* fields[LC_RADIO_FIELDS - 2];
    int   count = 0;
    char* cursor = buf;
    fields[count++] = cursor;
    while (count < LC_RADIO_FIELDS - 2 && (cursor = strchr(cursor, '|')) != NULL) {
        *cursor++       = '\0';
        fields[count++] = cursor;
    }
    if (count != LC_RADIO_FIELDS - 2)
        return 0;

    memset(out, 0, sizeof(*out));
    if (!fields[0][0] || strlen(fields[0]) >= sizeof(out->token))
        return 0;
    strcpy(out->token, fields[0]);

    char* end;
    out->playerId = (int)strtol(fields[1], &end, 10);
    if (*end || out->playerId <= 0)
        return 0;
    out->on        = atoi(fields[2]) != 0;
    out->frequency = atoi(fields[3]);
    out->range     = (float)atof(fields[4]);
    for (int i = 0; i < 3; ++i)
        out->pos[i] = (float)atof(fields[5 + i]);
    strncpy(out->key, fields[8], sizeof(out->key) - 1);
    return out->frequency > 0 && out->range > 0.0f && isfinite(out->pos[0]) && isfinite(out->pos[1]) && isfinite(out->pos[2]);
}

float lc_radio_effective_distance(float distance, float clearance)
{
    if (clearance <= 0.0f)
        return distance;
    const float terrain = clearance * LC_RADIO_TERRAIN_COEFFICIENT;
    return distance + terrain + terrain * (distance / 2000.0f);
}

float lc_radio_range_ratio(float distance, float clearance, float range)
{
    if (range <= 0.0f)
        return 2.0f;
    return lc_radio_effective_distance(distance, clearance) / range;
}

float lc_radio_quality(float distance, float clearance, float range, float cleanFraction)
{
    if (range <= 0.0f)
        return 0.0f;
    if (!(cleanFraction >= 0.0f))
        cleanFraction = LC_RADIO_CLEAN_FRACTION;
    else if (cleanFraction > LC_RADIO_MAX_CLEAN_FRACTION)
        cleanFraction = LC_RADIO_MAX_CLEAN_FRACTION;
    const float ratio = lc_radio_range_ratio(distance, clearance, range);
    if (ratio >= 1.0f)
        return 0.0f;
    if (ratio <= cleanFraction)
        return 1.0f;
    /* Straight line from clean to the edge: quality is how far through the garbled band you are. Curving
       it left most of the band sounding the same and crammed the change into the last stretch. */
    const float t = (ratio - cleanFraction) / (1.0f - cleanFraction);
    return 1.0f - (1.0f - LC_RADIO_EDGE_QUALITY) * t;
}
