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
/* TFAR's terrain model: effective distance = d + h*k + h*k*(d/2000), h being how far the path must rise to clear
   the terrain between the two ends. TFAR's default k is 7. */
#define LC_RADIO_TERRAIN_COEFFICIENT 7.0f

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
float lc_radio_effective_distance(float distance, float clearance);

/* Effective distance as a share of range: 0 at the transmitter, 1 at the edge, more than 1 out of range. */
float lc_radio_range_ratio(float distance, float clearance, float range);

/* 0 when out of range, otherwise signal quality in (0, 1], where 1 is a clean signal. cleanFraction is how
   much of the range stays perfectly clear before garbling starts. */
float lc_radio_quality(float distance, float clearance, float range, float cleanFraction);
