#pragma once

/* Parsed form of game_state.json, written by the game about 20 times a second (protocol 9):
{
  "v": 9, "seq": 42, "inGame": true,
  "session": { "token": "1789377566-123-456-789", "playerId": 1, "playerName": "Name",
               "tsChannel": "LimaCharlie", "tsChannelPassword": "",
               "modVersion": "1.0.0" },
  "self": { "alive": true, "pos": [x, y, z], "dir": [x, y, z], "tx": 2, "txFrequency": 45000, "txRadio": "123:1",
            "voiceRange": 20, "cleanFraction": 0.35, "beepFraction": 0.9, "unlimitedRx": false, "roomVolume": 96,
            "radios": [ { "id": "123:1", "freq": 45000, "range": 1500, "key": "US", "rx": true, "ear": 1, "volume": 0.8,
                          "beep": "tfar_sw", "halfDuplex": 0 } ],
            "beepVolume": 1,
            "sounds": [ { "seq": 3, "set": "ui", "name": "deny", "ear": 0, "volume": 1 } ] },
  "players": [ { "id": 2, "alive": true, "pos": [x, y, z], "muffle": 0.6, "room": 0.35 } ],
  "links": [ { "id": 2, "clearance": 35 } ]
}
self.pos/dir are the listener (camera); players carry no facing, as only the listener's matters.
players[].pos is the speaker's head, and muffle (0 clear .. 1) is
how obstructed the path between the two is. tx mirrors the game's EVONTransmitType: 0 none, 1 direct,
2 radio channel, 3 long range radio, and txRadio is the id in "radios" being transmitted on.
cleanFraction is the server's clean range setting: the share of a radio's range that stays perfectly clear
before the signal starts to garble. It never changes a radio's range, only where garbling begins.
beepFraction is how far out the start and end beeps of someone else's transmission are still heard, as a
share of range; past it the voice arrives with no beeps.
unlimitedRx is set while a Game Master has the editor open and the server allows it: every radio they are
tuned to receives at unlimited range, so distance and terrain stop mattering for what they hear. Their own
transmissions are made unlimited by the game announcing a limitless range on radios[] instead, which needs
nothing from the plugin.
beepVolume is one setting for every channel, 0..1, which scales each radio's own volume wherever a beep is
played; it does not touch the voice itself.
radios[] are the local player's transceivers: rx is false when switched off or muted, ear is 0 both, 1 left,
2 right, volume is 0..1 and beep names a sound set folder. halfDuplex marks a radio that cannot listen while
it transmits: it is deaf for as long as it is the radio being transmitted on. "sounds" holds the most recent sounds the game asked
for (UI tones, sample beeps) with increasing seq; the plugin plays each seq once. "players" holds only players near enough to matter for direct speech. "links"
answers the plugin's radioRx requests with the TFAR-style terrain clearance in metres (0 = line of sight)
between that transmitting player and the local player, already scaled by the server's terrain effect
setting: the terrain penalty is linear in the clearance, so the game scales it there rather than sending
the coefficient across. */

#define LC_GAME_STATE_MAX_PLAYERS 128
#define LC_GAME_STATE_MAX_RADIOS 16
#define LC_GAME_STATE_MAX_LINKS 64
#define LC_TOKEN_CAP 64
#define LC_RADIO_ID_CAP 32
#define LC_RADIO_KEY_CAP 64
#define LC_BEEP_SET_CAP 32
#define LC_SOUND_NAME_CAP 32
#define LC_GAME_STATE_MAX_SOUNDS 8

enum lc_tx_type {
    LC_TX_NONE,
    LC_TX_DIRECT,
    LC_TX_CHANNEL,
    LC_TX_LONG_RANGE
};

enum lc_ear {
    LC_EAR_BOTH,
    LC_EAR_LEFT,
    LC_EAR_RIGHT
};

typedef struct {
    int   id;
    int   alive;
    float pos[3];
    float muffle;
    /* 0 .. 1, how much of the listener's room this voice fills: all of it from inside the same room, a
       little from elsewhere in the building, next to none from outdoors. Scales the reverb send only. */
    float room;
} lc_player_state;

typedef struct {
    char  id[LC_RADIO_ID_CAP];
    int   frequency;
    float range;
    char  key[LC_RADIO_KEY_CAP];
    int   rx;
    int   ear;
    float volume;
    char  beep[LC_BEEP_SET_CAP];
    int   halfDuplex;
} lc_radio_state;

typedef struct {
    int   seq;
    char  set[LC_BEEP_SET_CAP];
    char  name[LC_SOUND_NAME_CAP];
    int   ear;
    float volume;
} lc_sound_event;

typedef struct {
    int   playerId;
    float clearance;
} lc_link_state;

typedef struct {
    long long seq;
    int       inGame;

    char token[LC_TOKEN_CAP];
    int  playerId;
    char playerName[128];
    char tsChannel[256];
    char tsChannelPassword[128];
    char modVersion[32];

    int   alive;
    float pos[3];
    float dir[3];
    int   tx;
    int   txFrequency;
    char  txRadio[LC_RADIO_ID_CAP];
    float voiceRange;
    float cleanFraction;
    float beepFraction;
    int   unlimitedRx;
    /* Volume in cubic metres of the room the listener is standing in, 0 outdoors: sizes the room reverb */
    float roomVolume;
    /* 0 .. 1, the player's beep volume for every channel, on top of each radio's own volume */
    float beepVolume;

    int             radioCount;
    lc_radio_state radios[LC_GAME_STATE_MAX_RADIOS];

    int             soundCount;
    lc_sound_event sounds[LC_GAME_STATE_MAX_SOUNDS];

    int              playerCount;
    lc_player_state players[LC_GAME_STATE_MAX_PLAYERS];

    int            linkCount;
    lc_link_state links[LC_GAME_STATE_MAX_LINKS];
} lc_game_state;

/* Returns 1 if json is a complete game state of the supported protocol version. */
int lc_game_state_parse(const char* json, lc_game_state* out);
