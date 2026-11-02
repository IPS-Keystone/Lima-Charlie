#pragma once

/* Radio reception maths and the transmission announcement exchanged between plugins. A transmitting plugin
   sends "LC1|RTX|<session token>|<player id>|<on 1/0>|<frequency kHz>|<range m>|<x>|<y>|<z>|<radio id>|<source>|
   <encryption key>" to its channel when it starts, changes radio or moves, every second while transmitting, and
   once more with on=0 when it stops. x, y, z is the transmitting radio's antenna (radios[].pos), the radio id is
   its radios[].id and the source its radios[].src by name (protocol 10; before it there were neither, and the
   position was the talker's). Receiving plugins decide what they hear from their own radios (trust model as
   TFAR). */

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
    float pos[3]; /* the transmitting radio's antenna */
    char  radioId[LC_RADIO_ID_CAP];
    int   src; /* lc_radio_source */
    char  key[LC_RADIO_KEY_CAP];
} lc_radio_announcement;

size_t lc_radio_format_announcement(char* out, size_t cap, const char* token, int playerId, int on, int frequency, float range, const float pos[3], const char* radioId, int src, const char* key);

/* Returns 1 if command is a well-formed RTX announcement. */
int lc_radio_parse_announcement(const char* command, lc_radio_announcement* out);

/* Where a transmission comes from, for distance and terrain: the antenna it announced, except that a radio on
   the talker's body uses the talker's head from our own game state when they are near enough to be in it,
   which is more current than the last announcement. player may be NULL. */
const float* lc_radio_transmitter_position(const lc_radio_announcement* info, const lc_player_state* player);

/* Metres from one of our receiving radios to a transmitting antenna. Measured from the receiving radio's own
   antenna (radios[].pos: self.pos for a radio on the body, the vehicle's antenna for a vehicle radio, the
   radio itself for a borrowed one), never from where we stand, so every radio of ours hears from where it is. */
float lc_radio_receiver_distance(const lc_radio_state* radio, const float transmitter[3]);

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

/* ---- Received signal: the one place that decides how a transmission reaches one receiver ----
   Everything that changes what a radio transmission sounds like at the receiving end goes through
   lc_radio_receive. New terms are new fields of lc_rx_inputs: a caller that zeroes the struct
   (memset or "= {0}") and fills only what it knows keeps today's behaviour for every field it leaves at 0. */

/* How long radio voice stays silent waiting for the game's terrain answer before assuming line of sight. */
#define LC_RADIO_LINK_WAIT_MS 300

typedef struct {
    /* Metres from the receiver to the transmitting antenna (lc_radio_transmitter_position): the RTX antenna
       position, or for a radio on the talker's body their game position while they are in our game state. */
    float distance;
    /* The transmitter's range in metres as its RTX announced it. Which range that is (TX1 long range, TX2
       short range, a Game Master's unlimited range) the transmitting side has already decided; the receiver
       takes it as it comes. */
    float range;
    /* Non-zero while the local player is a Game Master with unlimited reception (game state unlimitedRx):
       range is replaced by LC_RADIO_UNLIMITED_RANGE_M, so distance and terrain shrink to nothing against it. */
    int unlimitedRx;
    /* Non-zero once the game has traced the terrain to this transmitting radio (a links[] entry for its player
       and radio id exists). While 0, terrain is taken as line of sight. */
    int terrainKnown;
    /* links[].hills: the height in metres of every hill above the line of sight, highest first (the game
       state parser turns an old game's single "clearance" into one hill). Ignored unless terrainKnown;
       NULL / 0 is line of sight. Each hill takes lc_radio_terrain_share of what the others leave, capped at
       LC_RADIO_TERRAIN_MAX_LOSS in total. */
    const float* hills;
    int          hillCount;
    /* Milliseconds since this transmission started, as now - start in unsigned arithmetic (a clock that went
       backwards reads huge, so it does not wait). Only matters while terrain is not known: under
       LC_RADIO_LINK_WAIT_MS the voice is held back, beeps still play. */
    unsigned long long sinceStartMs;
    /* Game state cleanFraction: share of the range that stays perfectly clean. Out of 0 .. the max it falls
       back as lc_radio_quality does. */
    float cleanFraction;
    /* Game state beepFraction: share of the range within which the start and end beeps are heard. */
    float beepFraction;

    /* Planned terms, NOT implemented — each is a field here and a term in lc_radio_receive when it lands:
       - Jam level at the transmitter (EW): rides in RTX; noise added to the transmission for every receiver.
       - Jam level at the receiver (EW): from game state jammers[] reaching us on this frequency band.
       - Relay path (EW): best of direct and transmitter -> relay -> receiver, a relayed path's quality being
         the worse of its two legs (each leg its own distance, range and terrain).
       - SATCOM flag (EW): set from RTX; skips range, terrain and jamming entirely. */
} lc_rx_inputs;

typedef struct {
    /* 0 when distance >= the range in use: the transmission is not heard, not reported and not traced. Every
       other field is only meaningful when this is 1. */
    int inRange;
    /* Range the distance was measured against: the announced one, or LC_RADIO_UNLIMITED_RANGE_M. */
    float range;
    /* Share of range the terrain takes, 0 .. LC_RADIO_TERRAIN_MAX_LOSS (0 while terrain is not known). */
    float terrainLoss;
    /* Effective distance as a share of range (lc_radio_range_ratio). */
    float rangeRatio;
    /* lc_radio_quality: 0 when terrain puts it past range, otherwise (0, 1], 1 clean. Garbles the voice. */
    float quality;
    /* Non-zero when the start and end beeps are heard (rangeRatio within beepFraction). */
    int beepsAudible;
    /* Non-zero while the voice is held back waiting for the terrain trace. */
    int voiceHeld;
} lc_rx_result;

/* Decides how one transmission reaches this receiver. Never fails; out is fully written. */
void lc_radio_receive(const lc_rx_inputs* in, lc_rx_result* out);
