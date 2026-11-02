#include "lc_radio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LC_RADIO_FIELDS 13

/* Copies text with every one of the separators in it replaced by '_' */
static void copy_safe(char* out, size_t cap, const char* text, const char* separators)
{
    size_t i = 0;
    for (; text && text[i] && i + 1 < cap; ++i)
        out[i] = strchr(separators, text[i]) ? '_' : text[i];
    out[i] = '\0';
}

size_t lc_radio_format_announcement(char* out, size_t cap, const char* token, int playerId, int on, int frequency, float range, const float pos[3], const char* radioId, int src, const char* key)
{
    /* The key is the last field, so only a separator inside it could break parsing. The radio id also goes on
       to the game in radioRx, so it must not carry that format's separators either. */
    char safeKey[LC_RADIO_KEY_CAP];
    char safeRadio[LC_RADIO_ID_CAP];
    copy_safe(safeKey, sizeof(safeKey), key, "|");
    copy_safe(safeRadio, sizeof(safeRadio), radioId, "|,;");

    const int n = _snprintf(out, cap, LC_RADIO_COMMAND_PREFIX "%s|%d|%d|%d|%.0f|%.1f|%.1f|%.1f|%s|%s|%s", token, playerId, on ? 1 : 0, frequency, range, pos[0], pos[1], pos[2], safeRadio, lc_radio_source_name(src), safeKey);
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

    /* Split into the 11 fields after the prefix; the key keeps the remainder. */
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
    copy_safe(out->radioId, sizeof(out->radioId), fields[8], ",;");
    out->src = lc_radio_source_parse(fields[9]);
    strncpy(out->key, fields[10], sizeof(out->key) - 1);
    return out->frequency > 0 && out->range > 0.0f && isfinite(out->pos[0]) && isfinite(out->pos[1]) && isfinite(out->pos[2]);
}

const float* lc_radio_transmitter_position(const lc_radio_announcement* info, const lc_player_state* player)
{
    return player && lc_radio_source_on_body(info->src) ? player->pos : info->pos;
}

float lc_radio_receiver_distance(const lc_radio_state* radio, const float transmitter[3])
{
    const float dx = radio->pos[0] - transmitter[0];
    const float dy = radio->pos[1] - transmitter[1];
    const float dz = radio->pos[2] - transmitter[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float lc_radio_terrain_share(float height)
{
    if (!(height > 0.0f))
        return 0.0f;
    return LC_RADIO_TERRAIN_MAX_SHARE * (1.0f - (float)exp(-height / LC_RADIO_TERRAIN_SCALE_M));
}

float lc_radio_terrain_loss(const float* hills, int count)
{
    float left = 1.0f;
    for (int i = 0; hills && i < count; ++i)
        left *= 1.0f - lc_radio_terrain_share(hills[i]);
    const float loss = 1.0f - left;
    return loss > LC_RADIO_TERRAIN_MAX_LOSS ? LC_RADIO_TERRAIN_MAX_LOSS : loss;
}

float lc_radio_effective_distance(float distance, float terrainLoss)
{
    if (!(terrainLoss > 0.0f))
        return distance;
    if (terrainLoss > LC_RADIO_TERRAIN_MAX_LOSS)
        terrainLoss = LC_RADIO_TERRAIN_MAX_LOSS;
    /* The range shrinking by the loss is the same as the distance growing by 1 / (1 - loss) */
    return distance / (1.0f - terrainLoss);
}

float lc_radio_range_ratio(float distance, float terrainLoss, float range)
{
    if (range <= 0.0f)
        return 2.0f;
    return lc_radio_effective_distance(distance, terrainLoss) / range;
}

float lc_radio_quality(float distance, float terrainLoss, float range, float cleanFraction)
{
    if (range <= 0.0f)
        return 0.0f;
    if (!(cleanFraction >= 0.0f))
        cleanFraction = LC_RADIO_CLEAN_FRACTION;
    else if (cleanFraction > LC_RADIO_MAX_CLEAN_FRACTION)
        cleanFraction = LC_RADIO_MAX_CLEAN_FRACTION;
    const float ratio = lc_radio_range_ratio(distance, terrainLoss, range);
    if (ratio >= 1.0f)
        return 0.0f;
    if (ratio <= cleanFraction)
        return 1.0f;
    /* Straight line from clean to the edge: quality is how far through the garbled band you are. Curving
       it left most of the band sounding the same and crammed the change into the last stretch. */
    const float t = (ratio - cleanFraction) / (1.0f - cleanFraction);
    return 1.0f - (1.0f - LC_RADIO_EDGE_QUALITY) * t;
}

void lc_radio_receive(const lc_rx_inputs* in, lc_rx_result* out)
{
    memset(out, 0, sizeof(*out));
    /* A Game Master hears the whole map: the range everything is measured against is the only thing that
       changes, so distance and terrain both shrink to nothing against it. */
    out->range = in->unlimitedRx ? LC_RADIO_UNLIMITED_RANGE_M : in->range;
    if (in->distance >= out->range)
        return;
    out->inRange = 1;

    /* Until the game has traced the terrain, assume line of sight for the beeps but keep the voice silent. */
    if (in->terrainKnown)
        out->terrainLoss = lc_radio_terrain_loss(in->hills, in->hillCount);
    out->voiceHeld = !in->terrainKnown && in->sinceStartMs < LC_RADIO_LINK_WAIT_MS;

    out->quality    = lc_radio_quality(in->distance, out->terrainLoss, out->range, in->cleanFraction);
    out->rangeRatio = lc_radio_range_ratio(in->distance, out->terrainLoss, out->range);
    /* Beeps stop short of the voice, so a transmission from the far edge is speech without beeps. */
    out->beepsAudible = out->rangeRatio <= in->beepFraction;

    /* Jamming (both ends), relay paths and SATCOM land here; see lc_rx_inputs. */
}
