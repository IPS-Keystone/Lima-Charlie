#pragma once

/* Radio reception maths and the transmission announcement exchanged between plugins. A transmitting plugin
   sends "LC1|RTX|<session token>|<player id>|<on 1/0>|<frequency kHz>|<range m>|<x>|<y>|<z>|<encryption key>"
   to its channel when it starts, changes radio or moves, every second while transmitting, and once more with
   on=0 when it stops. Receiving plugins decide what they hear from their own radios (trust model as TFAR). */

#include "lc_game_state.h"

#include <stddef.h>

#define LC_RADIO_COMMAND_PREFIX "LC1|RTX|"

/* Signal is perfect up to this fraction of the transmitter's range, then garbles progressively to the edge.
   The game can move this point with its clean range setting; this is the default it sends. */
#define LC_RADIO_CLEAN_FRACTION 0.35f
/* Leaving no garbled band at all would make range a hard cliff, so the clean part stops short of the edge. */
#define LC_RADIO_MAX_CLEAN_FRACTION 0.95f
/* Worst quality still heard, right at the edge of range. */
#define LC_RADIO_EDGE_QUALITY 0.05f
/* Share of the range within which the start and end beeps of someone else's transmission are heard. The
   game sends its own value; this is the default. Below the voice cutoff, so a distant transmission
   arrives as speech with no beeps rather than beeping when it is barely readable. */
#define LC_RADIO_BEEP_FRACTION 0.9f

/* Range a Game Master transmits and receives at while the editor is open: far past any map, so both distance
   and terrain come out as nothing once they are divided by it. Matches LC_Radio.UNLIMITED_RANGE_M. */
#define LC_RADIO_UNLIMITED_RANGE_M 1000000.0f
/* Each hill above the line between the two ends takes a share of the radio's range: share = max * (1 - e^(-h /
   scale)), h being its height above that line. The first metres of a hill count most, and no single hill can
   take more than the maximum. Several hills each take their share of what the others leave, so hills of 30
   and 60 m leave (1 - share of 30) * (1 - share of 60) of the range. The range shrinks rather than the
   distance growing, so the clean part and the garbled band shrink with it, and however much terrain is in
   the way it never takes more than the total cap. */
#define LC_RADIO_TERRAIN_MAX_SHARE 0.8f
#define LC_RADIO_TERRAIN_SCALE_M 95.0f
#define LC_RADIO_TERRAIN_MAX_LOSS 0.9f

typedef struct {
    char  token[LC_TOKEN_CAP];
    int   playerId;
    int   on;
    int   frequency;
    float range;
    float pos[3];
    char  key[LC_RADIO_KEY_CAP];
} lc_radio_announcement;

size_t lc_radio_format_announcement(char* out, size_t cap, const char* token, int playerId, int on, int frequency, float range, const float pos[3], const char* key);

/* Returns 1 if command is a well-formed RTX announcement. */
int lc_radio_parse_announcement(const char* command, lc_radio_announcement* out);

/* Distance after terrain: clearance is the TFAR-style terrain height in metres (0 for line of sight). */
/* Share of a radio's range that one hill this many metres above the line of sight takes,
   0 .. LC_RADIO_TERRAIN_MAX_SHARE. */
float lc_radio_terrain_share(float height);

/* Share of the range every hill on the path takes together, 0 .. LC_RADIO_TERRAIN_MAX_LOSS. */
float lc_radio_terrain_loss(const float* hills, int count);

/* Distance after terrain, terrainLoss being lc_radio_terrain_loss (0 for line of sight). */
float lc_radio_effective_distance(float distance, float terrainLoss);

/* Effective distance as a share of range: 0 at the transmitter, 1 at the edge, more than 1 out of range. */
float lc_radio_range_ratio(float distance, float terrainLoss, float range);

/* 0 when out of range, otherwise signal quality in (0, 1], where 1 is a clean signal. cleanFraction is how
   much of the range stays perfectly clear before garbling starts. */
float lc_radio_quality(float distance, float terrainLoss, float range, float cleanFraction);
