#include "lc_core.h"

#include "lc_audio.h"
#include "lc_direct_voice.h"
#include "lc_game_state.h"
#include "lc_log.h"
#include "lc_peers.h"
#include "lc_profile_files.h"
#include "lc_radio.h"
#include "lc_reverb.h"
#include "lc_sounds.h"
#include "lc_transmissions.h"
#include "lc_version.h"

#include <windows.h>
#include <timeapi.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LC_POLL_MS 5
/* Nothing here is latency-critical until a game is running, and TeamSpeak often runs for hours without one.
   While idle the worker polls slowly and leaves the system timer resolution alone. */
#define LC_IDLE_POLL_MS 50
/* No write for this long means the game is not ticking: alt-tabbed, minimised, hitching, or gone. Only the
   radio transmission ends on it, so a frozen game cannot hold a transmit key open on the net. */
#define LC_GAME_STALE_MS 3000
/* How long a game that stopped writing is still treated as being played, for the channel, the peers and the
   voices. Leaving the game properly writes inGame:false, which is immediate and is what normally ends a
   session, so this only decides how long a crashed or killed game keeps you in the game channel. Long,
   because being dragged out of the channel and back in every time someone alt-tabs is worse than sitting in
   the mission channel for a few minutes after a crash: every move is a join and leave notification for
   everyone else in it. */
#define LC_GAME_GRACE_MS 300000
#define LC_STATE_HEARTBEAT_MS 250
#define LC_STATE_MIN_INTERVAL_MS 20
#define LC_STATE_BODY_CAP 8192
#define LC_HELLO_INTERVAL_MS 10000
#define LC_PEER_MAX_AGE_MS 35000
#define LC_CHANNEL_CHECK_MS 2000
#define LC_MAX_TALKERS 256
#define LC_GAME_STATE_BUFFER (256 * 1024)

/* Own transmissions are re-announced this often, and sooner after moving this far. */
#define LC_RADIO_REFRESH_MS 1000
#define LC_RADIO_MOVE_REFRESH_M 25.0f
/* A re-key on the same radio within this long continues the transmission: no beeps, no stop announcement. */
#define LC_TX_STOP_DEBOUNCE_MS 300
/* Announcements not refreshed for this long are dropped (lost stop, crashed sender). */
#define LC_TRANSMISSION_MAX_AGE_MS 3500
/* How long radio voice stays silent waiting for the game's terrain answer before assuming line of sight. */
#define LC_LINK_WAIT_MS 300
#define LC_MAX_RECEPTIONS 64
/* Radio voice does not get quieter with distance, only more garbled. */
#define LC_RADIO_VOICE_GAIN 0.8f
#define LC_RADIO_RX_CAP 4096

static HANDLE        g_thread = NULL;
static volatile LONG g_quit   = 0;

/* Clients talking on the current connection. Written by TeamSpeak callbacks, read by the worker. */
static CRITICAL_SECTION g_talkersLock;
static anyID            g_talkers[LC_MAX_TALKERS];
static int              g_talkerCount;

/* Set by callbacks when our HELLO should be broadcast soon (a peer or newcomer needs to learn us). */
static volatile LONG g_helloRequested;

/* Worker thread state */
static lc_game_state     g_game;
static int                g_haveGame;
static unsigned long long g_gameStamp;
static int                g_gameDir = -1;
static int                g_wasInGame;
/* Whether the state was being written on the previous loop, for the one log line each way */
static int                g_wasFresh = 1;
static uint64             g_sch;

static uint64             g_previousChannel;
static uint64             g_gameChannel;
static unsigned long long g_nextChannelCheckMs;
static char               g_missingChannelLogged[256];

static char               g_helloToken[LC_TOKEN_CAP];
static int                g_helloPlayerId = -1;
static float              g_helloVoiceRange;
static uint64             g_helloChannel;
static unsigned long long g_nextHelloMs;

/* Our own radio transmission, as last announced. */
static int                g_txActive;
static uint64             g_txSch;
static lc_radio_state    g_txRadio;
static char               g_txToken[LC_TOKEN_CAP];
static int                g_txPlayerId;
static float              g_txPos[3];
static unsigned long long g_txNextRefreshMs;
static int                g_txStopPending;
static unsigned long long g_txStopAtMs;

/* Radio transmissions currently heard, per sender and receiving radio, for the start and end beeps. */
typedef struct {
    anyID client;
    char  radioId[LC_RADIO_ID_CAP];
    char  beep[LC_BEEP_SET_CAP];
    int   ear;
    float volume;
} lc_reception;

static lc_reception g_receptions[LC_MAX_RECEPTIONS];
static int           g_receptionCount;
/* Newest game sound event already played; -1 until the first game state after entering a game. */
static int g_lastSoundSeq = -1;

/* "playerId,x,y,z;..." transmitters we need the terrain clearance to, reported to the game. */
static char g_radioRx[LC_RADIO_RX_CAP];

/* "playerId,radioId,frequency,quality;..." the transmissions we can actually hear and the local radio each
   arrives on, so the game can put them on the vanilla VON display. */
static char g_radioHeard[LC_RADIO_RX_CAP];

static long long          g_stateSeq;
static unsigned long long g_lastStateWriteMs;
static char               g_lastStateBody[LC_STATE_BODY_CAP];

/* Whether TeamSpeak itself is holding the microphone shut, by the local mute, the muted-microphone
   toggle, or a muted speaker, which implies the microphone too. A build before 1.0.7 gated the microphone
   to follow the game's transmit keys, and one that stopped while it was shut could leave it that way, with
   nothing on screen to say so. */
static int read_mic_muted(uint64 sch)
{
    int value = 0;
    if (g_ts3.getClientSelfVariableAsInt(sch, CLIENT_INPUT_DEACTIVATED, &value) == ERROR_ok && value != INPUT_ACTIVE)
        return 1;

    if (g_ts3.getClientSelfVariableAsInt(sch, CLIENT_INPUT_MUTED, &value) == ERROR_ok && value != 0)
        return 1;

    if (g_ts3.getClientSelfVariableAsInt(sch, CLIENT_OUTPUT_MUTED, &value) == ERROR_ok && value != 0)
        return 1;

    return 0;
}

/* Three TeamSpeak calls, each taking its locks, are not worth making two hundred times a second for
   something only a person can change. Re-read no faster than the state file is written. */
static int is_mic_muted(uint64 sch, unsigned long long nowMs)
{
    static int                cached;
    static unsigned long long nextReadMs;
    if (nowMs >= nextReadMs) {
        nextReadMs = nowMs + LC_STATE_HEARTBEAT_MS;
        cached     = read_mic_muted(sch);
    }

    return cached;
}

static int is_connected(uint64 sch)
{
    int status = STATUS_DISCONNECTED;
    return sch && g_ts3.getConnectionStatus(sch, &status) == ERROR_ok && status == STATUS_CONNECTION_ESTABLISHED;
}

static int get_own_channel(uint64 sch, anyID* me, uint64* channel)
{
    return g_ts3.getClientID(sch, me) == ERROR_ok && *me != 0 && g_ts3.getChannelOfClient(sch, *me, channel) == ERROR_ok;
}

static uint64 find_channel_by_name(uint64 sch, const char* name)
{
    uint64* channels = NULL;
    if (g_ts3.getChannelList(sch, &channels) != ERROR_ok || !channels)
        return 0;
    uint64 found = 0;
    for (size_t i = 0; channels[i] && !found; ++i) {
        char* channelName = NULL;
        if (g_ts3.getChannelVariableAsString(sch, channels[i], CHANNEL_NAME, &channelName) == ERROR_ok && channelName) {
            if (strcmp(channelName, name) == 0)
                found = channels[i];
            g_ts3.freeMemory(channelName);
        }
    }
    g_ts3.freeMemory(channels);
    return found;
}

static const lc_player_state* find_player(int playerId)
{
    for (int i = 0; i < g_game.playerCount; ++i) {
        if (g_game.players[i].id == playerId)
            return &g_game.players[i];
    }
    return NULL;
}

static const lc_radio_state* find_radio(const char* id)
{
    if (!id[0])
        return NULL;
    for (int i = 0; i < g_game.radioCount; ++i) {
        if (strcmp(g_game.radios[i].id, id) == 0)
            return &g_game.radios[i];
    }
    return NULL;
}

static int find_link(int playerId, float* clearance)
{
    for (int i = 0; i < g_game.linkCount; ++i) {
        if (g_game.links[i].playerId == playerId) {
            *clearance = g_game.links[i].clearance;
            return 1;
        }
    }
    return 0;
}

static float distance3(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

static void clear_talkers(void)
{
    EnterCriticalSection(&g_talkersLock);
    g_talkerCount = 0;
    LeaveCriticalSection(&g_talkersLock);
}

static void poll_game_state(char* buf, lc_game_state* parsed)
{
    size_t             len   = 0;
    int                dir   = -1;
    unsigned long long stamp = 0;
    if (!lc_profile_poll_game_state(buf, LC_GAME_STATE_BUFFER, &len, &dir, &stamp))
        return;
    if (!lc_game_state_parse(buf, parsed))
        return; /* probably mid-write; read again next poll */

    lc_profile_consume_game_state(stamp);
    memcpy(&g_game, parsed, sizeof(g_game));
    g_haveGame  = 1;
    g_gameStamp = stamp;
    if (g_gameDir != dir) {
        g_gameDir = dir;
        lc_logf(LC_LOG_INFO, "Game state found in %s profile", lc_profile_dir_name(dir));
    }
}

/* Milliseconds since the game last wrote its state, 0 if it has never been seen or the clock went backwards. */
static unsigned long long game_state_age_ms(void)
{
    const unsigned long long now = lc_profile_now_stamp();
    if (now < g_gameStamp)
        return 0;

    return (now - g_gameStamp) / 10000ULL;
}

/* In a game, and the state is being written: everything may act on it. */
static int game_fresh(void)
{
    return g_haveGame && g_game.inGame && game_state_age_ms() < LC_GAME_STALE_MS;
}

/* In a game as far as anyone can tell. The game says so and has not been silent long enough to be written
   off, whether or not it is writing right now. */
static int game_present(void)
{
    return g_haveGame && g_game.inGame && game_state_age_ms() < LC_GAME_GRACE_MS;
}

void lc_core_status(lc_status* out)
{
    memset(out, 0, sizeof(*out));
    /* g_game is only replaced wholesale by the worker, so a torn read would at worst show stale text. */
    out->inGame = game_present();
    if (!out->inGame)
        return;

    out->playing  = g_game.alive;
    out->playerId = g_game.playerId;
    strncpy(out->playerName, g_game.playerName, sizeof(out->playerName) - 1);
    strncpy(out->modVersion, g_game.modVersion, sizeof(out->modVersion) - 1);
    strncpy(out->token, g_game.token, sizeof(out->token) - 1);
}

static void on_connection_switched(uint64 sch)
{
    lc_peers_reset();
    lc_transmissions_reset();
    lc_audio_reset();
    clear_talkers();
    g_previousChannel    = 0;
    g_gameChannel        = 0;
    g_helloPlayerId      = -1;
    g_helloToken[0]      = '\0';
    g_helloChannel       = 0;
    g_nextChannelCheckMs = 0;
    g_txActive           = 0;
    g_txStopPending      = 0;
    g_receptionCount     = 0;
    g_sch                = sch;
}

static void update_channel(uint64 sch, int connected, int inGame, unsigned long long nowMs)
{
    if (!inGame || !connected || !g_game.tsChannel[0] || nowMs < g_nextChannelCheckMs)
        return;
    g_nextChannelCheckMs = nowMs + LC_CHANNEL_CHECK_MS;

    anyID  me      = 0;
    uint64 current = 0;
    if (!get_own_channel(sch, &me, &current))
        return;

    const uint64 target = find_channel_by_name(sch, g_game.tsChannel);
    if (!target) {
        if (strcmp(g_missingChannelLogged, g_game.tsChannel) != 0) {
            lc_logf(LC_LOG_WARNING, "TeamSpeak channel \"%s\" set by the game server was not found", g_game.tsChannel);
            strncpy(g_missingChannelLogged, g_game.tsChannel, sizeof(g_missingChannelLogged) - 1);
        }
        return;
    }
    g_missingChannelLogged[0] = '\0';
    g_gameChannel             = target;
    if (current == target)
        return;

    if (!g_previousChannel)
        g_previousChannel = current;
    const unsigned int err = g_ts3.requestClientMove(sch, me, target, g_game.tsChannelPassword, NULL);
    if (err == ERROR_ok)
        lc_logf(LC_LOG_INFO, "Moving to game channel \"%s\"", g_game.tsChannel);
    else
        lc_logf(LC_LOG_WARNING, "Could not move to game channel \"%s\" (error %u)", g_game.tsChannel, err);
}

static void on_left_game(uint64 sch, int connected)
{
    anyID  me      = 0;
    uint64 current = 0;
    if (connected && g_previousChannel && g_gameChannel && get_own_channel(sch, &me, &current) && current == g_gameChannel) {
        const unsigned int err = g_ts3.requestClientMove(sch, me, g_previousChannel, "", NULL);
        lc_logf(err == ERROR_ok ? LC_LOG_INFO : LC_LOG_WARNING, "Returning to previous channel (result %u)", err);
    }
    g_previousChannel = 0;
    g_gameChannel     = 0;
    g_helloPlayerId   = -1;
    g_helloToken[0]   = '\0';
    lc_peers_reset();
    lc_transmissions_reset();
    lc_audio_reset();
    lc_logf(LC_LOG_INFO, "Left game");
}

static void update_handshake(uint64 sch, int inGame, int haveChannel, uint64 channel, unsigned long long nowMs)
{
    const LONG requested = InterlockedExchange(&g_helloRequested, 0);
    if (!inGame || !haveChannel || !g_game.token[0] || g_game.playerId <= 0 || !lc_plugin_id())
        return;

    const int identityChanged = strcmp(g_helloToken, g_game.token) != 0 || g_helloPlayerId != g_game.playerId || g_helloChannel != channel || fabs(g_helloVoiceRange - g_game.voiceRange) > 0.05f;
    if (!identityChanged && !requested && nowMs < g_nextHelloMs)
        return;

    char command[128];
    if (!lc_peers_format_hello(command, sizeof(command), g_game.token, g_game.playerId, g_game.voiceRange,
                               LC_PLUGIN_VERSION, g_game.modVersion))
        return;
    g_ts3.sendPluginCommand(sch, lc_plugin_id(), command, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);

    strncpy(g_helloToken, g_game.token, sizeof(g_helloToken) - 1);
    g_helloPlayerId   = g_game.playerId;
    g_helloVoiceRange = g_game.voiceRange;
    g_helloChannel    = channel;
    g_nextHelloMs     = nowMs + LC_HELLO_INTERVAL_MS;
}

static void send_radio_announcement(uint64 sch, int on)
{
    char command[256];
    if (!lc_plugin_id() || !lc_radio_format_announcement(command, sizeof(command), g_txToken, g_txPlayerId, on, g_txRadio.frequency, g_txRadio.range, g_txPos, g_txRadio.key))
        return;
    g_ts3.sendPluginCommand(sch, lc_plugin_id(), command, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
}

static void finish_radio_tx(void)
{
    g_txActive      = 0;
    g_txStopPending = 0;
    if (is_connected(g_txSch))
        send_radio_announcement(g_txSch, 0);
}

/* Announces our own radio transmission to the channel and plays our local beeps. A release is held for
   LC_TX_STOP_DEBOUNCE_MS, so hammering push-to-talk on one radio reads as one transmission to everyone. */
static void update_radio_tx(uint64 sch, int connected, int inGame, unsigned long long nowMs)
{
    const lc_radio_state* radio = NULL;
    if (inGame && connected && g_game.alive && g_game.token[0] && g_game.playerId > 0 && (g_game.tx == LC_TX_CHANNEL || g_game.tx == LC_TX_LONG_RANGE))
        radio = find_radio(g_game.txRadio);

    if (radio) {
        /* Key pressed again inside the debounce: the announcement never stopped, so only the beep restarts. */
        const int resumed = g_txStopPending;
        if (g_txStopPending) {
            const int sameTransmission = g_txSch == sch && strcmp(radio->id, g_txRadio.id) == 0 && radio->frequency == g_txRadio.frequency && strcmp(radio->key, g_txRadio.key) == 0;
            g_txStopPending            = 0;
            if (!sameTransmission)
                finish_radio_tx();
        }

        const int started = !g_txActive || g_txSch != sch;
        const int changed = started || strcmp(radio->id, g_txRadio.id) != 0 || radio->frequency != g_txRadio.frequency || fabs(radio->range - g_txRadio.range) > 0.5f || strcmp(radio->key, g_txRadio.key) != 0 || strcmp(g_game.token, g_txToken) != 0;
        const int moved   = distance3(g_game.pos, g_txPos) > LC_RADIO_MOVE_REFRESH_M;
        if (started || resumed)
            lc_sounds_play(radio->beep, "local_start", radio->ear, beep_gain(radio->volume));

        g_txActive = 1;
        g_txSch    = sch;
        g_txRadio  = *radio;
        strncpy(g_txToken, g_game.token, sizeof(g_txToken) - 1);
        g_txPlayerId = g_game.playerId;
        if (changed || moved || nowMs >= g_txNextRefreshMs) {
            memcpy(g_txPos, g_game.pos, sizeof(g_txPos));
            send_radio_announcement(sch, 1);
            g_txNextRefreshMs = nowMs + LC_RADIO_REFRESH_MS;
        }
        return;
    }

    if (!g_txActive)
        return;
    if (!g_txStopPending) {
        g_txStopPending = 1;
        g_txStopAtMs    = nowMs + LC_TX_STOP_DEBOUNCE_MS;
        /* Your own beep follows the key, not the debounce, so releasing sounds immediate. */
        lc_sounds_play(g_txRadio.beep, "local_end", g_txRadio.ear, beep_gain(g_txRadio.volume));
    }
    if (!inGame || !connected || nowMs >= g_txStopAtMs)
        finish_radio_tx();
}

/* UI tones and sample beeps the game asked for. Events already in the file when joining are skipped. */
static void update_sound_events(int inGame)
{
    if (!inGame) {
        g_lastSoundSeq = -1;
        return;
    }
    int newest = g_lastSoundSeq < 0 ? 0 : g_lastSoundSeq;
    for (int i = 0; i < g_game.soundCount; ++i) {
        const lc_sound_event* sound = &g_game.sounds[i];
        if (g_lastSoundSeq >= 0 && sound->seq > g_lastSoundSeq)
            lc_sounds_play(sound->set, sound->name, sound->ear, sound->volume);
        if (sound->seq > newest)
            newest = sound->seq;
    }
    g_lastSoundSeq = newest;
}

static int find_reception(const lc_reception* list, int count, anyID client, const char* radioId)
{
    for (int i = 0; i < count; ++i) {
        if (list[i].client == client && strcmp(list[i].radioId, radioId) == 0)
            return i;
    }
    return -1;
}

/* A channel's beeps play at the channel's own volume scaled by the one beep volume the player sets for
   every channel. The voice itself is untouched by it. */
static float beep_gain(float radioVolume)
{
    return radioVolume * g_game.beepVolume;
}

/* Beeps for receptions that started or ended since the last update. */
static void update_receptions(const lc_reception* current, int count, int playSounds)
{
    if (playSounds) {
        for (int i = 0; i < count; ++i) {
            if (find_reception(g_receptions, g_receptionCount, current[i].client, current[i].radioId) < 0)
                lc_sounds_play(current[i].beep, "remote_start", current[i].ear, beep_gain(current[i].volume));
        }
        for (int i = 0; i < g_receptionCount; ++i) {
            if (find_reception(current, count, g_receptions[i].client, g_receptions[i].radioId) < 0)
                lc_sounds_play(g_receptions[i].beep, "remote_end", g_receptions[i].ear, beep_gain(g_receptions[i].volume));
        }
    }
    if (count > 0)
        memcpy(g_receptions, current, sizeof(*current) * (size_t)count);
    g_receptionCount = count;
}

static lc_voice_target* find_target(lc_voice_target* targets, int count, anyID client)
{
    for (int i = 0; i < count; ++i) {
        if (targets[i].client == client)
            return &targets[i];
    }
    return NULL;
}

/* Who can be heard, and how, from the latest game state: direct speech from nearby peers plus radio
   transmissions on frequencies our radios are tuned to. Everyone else in TeamSpeak is silenced while in game. */
static void publish_voice(int connected, int inGame, unsigned long long nowMs)
{
    static lc_peer_info    peers[LC_AUDIO_MAX_TARGETS];
    static lc_voice_target targets[LC_AUDIO_MAX_TARGETS];
    static lc_transmission transmissions[LC_MAX_TRANSMISSIONS];
    static lc_reception    receptions[LC_MAX_RECEPTIONS];

    g_radioRx[0]    = '\0';
    g_radioHeard[0] = '\0';
    if (!inGame || !connected) {
        update_receptions(NULL, 0, 0);
        lc_reverb_set_room(0.0f);
        lc_audio_publish(0, NULL, 0);
        return;
    }

    int    count          = 0;
    int    receptionCount = 0;
    size_t rxLen          = 0;
    size_t heardLen       = 0;
    if (g_game.alive && g_game.token[0]) {
        const int peerCount = lc_peers_list(g_game.token, peers, LC_AUDIO_MAX_TARGETS);
        for (int i = 0; i < peerCount; ++i) {
            const lc_player_state* player = find_player(peers[i].playerId);
            if (!player || !player->alive)
                continue;
            lc_voice_target* target = &targets[count];
            memset(target, 0, sizeof(*target));
            if (!lc_direct_voice_compute(g_game.pos, g_game.dir, player->pos, peers[i].voiceRange, player->muffle, &target->gainLeft, &target->gainRight))
                continue;
            target->client = peers[i].client;
            target->muffle = player->muffle;
            target->roomShare = player->room;
            ++count;
        }

        const int transmissionCount = lc_transmissions_list(g_game.token, transmissions, LC_MAX_TRANSMISSIONS);
        for (int i = 0; i < transmissionCount; ++i) {
            const lc_transmission* tx     = &transmissions[i];
            const lc_player_state* player = find_player(tx->info.playerId);
            if (player && !player->alive)
                continue;
            const float* speakerPos = player ? player->pos : tx->info.pos;
            const float  distance   = distance3(g_game.pos, speakerPos);
            /* A Game Master hears the whole map: the range everything is measured against is the only thing
               that changes, so distance and terrain both shrink to nothing against it. */
            const float range = g_game.unlimitedRx ? LC_RADIO_UNLIMITED_RANGE_M : tx->info.range;
            if (distance >= range)
                continue;

            /* Until the game has traced the terrain, assume line of sight for the beeps but keep the voice silent. */
            float     clearance = 0.0f;
            const int waiting   = !find_link(tx->info.playerId, &clearance) && nowMs - tx->startMs < LC_LINK_WAIT_MS;
            const float quality = lc_radio_quality(distance, clearance, range, g_game.cleanFraction);
            /* Beeps stop short of the voice, so a transmission from the far edge is speech without beeps. */
            const int beepsAudible = lc_radio_range_ratio(distance, clearance, range) <= g_game.beepFraction;

            float left  = 0.0f;
            float right = 0.0f;
            int   tuned = 0;
            int   heardRecorded = 0;
            for (int r = 0; r < g_game.radioCount; ++r) {
                const lc_radio_state* radio = &g_game.radios[r];
                if (!radio->rx || radio->frequency != tx->info.frequency || strcmp(radio->key, tx->info.key) != 0)
                    continue;
                /* A half duplex set is deaf while it is the one being transmitted on. Anything else we
                   carry on the same frequency still hears the traffic. */
                if (radio->halfDuplex && g_game.tx != LC_TX_NONE && strcmp(radio->id, g_game.txRadio) == 0)
                    continue;
                tuned = 1;
                if (quality <= 0.0f)
                    continue;
                /* Several radios on the frequency: each ear takes the loudest. */
                const float gain = LC_RADIO_VOICE_GAIN * radio->volume;
                if (radio->ear != LC_EAR_RIGHT && gain > left)
                    left = gain;
                if (radio->ear != LC_EAR_LEFT && gain > right)
                    right = gain;
                if (!heardRecorded) {
                    heardRecorded = 1;
                    const int n   = _snprintf(g_radioHeard + heardLen, sizeof(g_radioHeard) - heardLen, "%d,%s,%d,%.2f;", tx->info.playerId, radio->id, radio->frequency, quality);
                    if (n > 0 && heardLen + (size_t)n < sizeof(g_radioHeard))
                        heardLen += (size_t)n;
                    else
                        g_radioHeard[heardLen] = '\0';
                }
                if (beepsAudible && receptionCount < LC_MAX_RECEPTIONS) {
                    lc_reception* reception = &receptions[receptionCount++];
                    reception->client        = tx->client;
                    reception->ear           = radio->ear;
                    reception->volume        = radio->volume;
                    strcpy(reception->radioId, radio->id);
                    strcpy(reception->beep, radio->beep);
                }
            }

            if (tuned && rxLen + 64 < sizeof(g_radioRx)) {
                const int n = _snprintf(g_radioRx + rxLen, sizeof(g_radioRx) - rxLen, "%d,%.1f,%.1f,%.1f;", tx->info.playerId, speakerPos[0], speakerPos[1], speakerPos[2]);
                if (n > 0)
                    rxLen += (size_t)n;
            }
            if (waiting || (left <= 0.0f && right <= 0.0f))
                continue;

            lc_voice_target* target = find_target(targets, count, tx->client);
            if (!target) {
                if (count >= LC_AUDIO_MAX_TARGETS)
                    continue;
                target = &targets[count++];
                memset(target, 0, sizeof(*target));
                target->client = tx->client;
            }
            target->radioLeft    = left;
            target->radioRight   = right;
            target->radioQuality = quality;
        }
    }

    update_receptions(receptions, receptionCount, 1);
    lc_reverb_set_room(g_game.roomVolume);
    lc_audio_publish(1, targets, count);
}

static void write_plugin_state(int connected, int inGame, int haveChannel, int micMuted, anyID me, uint64 channel, unsigned long long nowMs)
{
    if (g_gameDir < 0)
        return;

    /* Reforger player ids of peers from this session who are talking right now. */
    char   talking[1024];
    size_t talkingLen  = 0;
    int    selfTalking = 0;
    talking[0]         = '\0';
    EnterCriticalSection(&g_talkersLock);
    for (int i = 0; i < g_talkerCount; ++i) {
        if (haveChannel && g_talkers[i] == me) {
            selfTalking = 1;
            continue;
        }
        const int playerId = g_game.token[0] ? lc_peers_player_id(g_talkers[i], g_game.token) : -1;
        if (playerId <= 0 || talkingLen + 16 >= sizeof(talking))
            continue;
        const int n = _snprintf(talking + talkingLen, sizeof(talking) - talkingLen, talkingLen ? ",%d" : "%d", playerId);
        if (n > 0)
            talkingLen += (size_t)n;
    }
    LeaveCriticalSection(&g_talkersLock);

    const int inGameChannel = haveChannel && g_gameChannel && channel == g_gameChannel;
    const int peers         = g_game.token[0] ? lc_peers_count(g_game.token) : 0;

    char      body[LC_STATE_BODY_CAP];
    const int bodyLen = _snprintf(body, sizeof(body),
        "\"pluginVersion\":\"%s\",\"inGame\":%s,\"tsConnected\":%s,\"tsClientId\":%u,\"inGameChannel\":%s,\"peers\":%d,\"selfTalking\":%s,\"micMuted\":%s,\"talking\":\"%s\",\"radioRx\":\"%s\",\"radioHeard\":\"%s\"",
        LC_PLUGIN_VERSION, inGame ? "true" : "false", connected ? "true" : "false", haveChannel ? (unsigned)me : 0U,
        inGameChannel ? "true" : "false", peers, selfTalking ? "true" : "false", micMuted ? "true" : "false", talking, g_radioRx, g_radioHeard);
    if (bodyLen <= 0 || (size_t)bodyLen >= sizeof(body))
        return;

    const unsigned long long sinceLastMs = nowMs - g_lastStateWriteMs;
    const int                changed     = strcmp(body, g_lastStateBody) != 0;
    if (!(changed && sinceLastMs >= LC_STATE_MIN_INTERVAL_MS) && sinceLastMs < LC_STATE_HEARTBEAT_MS)
        return;

    char      json[LC_STATE_BODY_CAP + 256];
    const int jsonLen = _snprintf(json, sizeof(json), "{\"v\":%d,\"seq\":%lld,\"gameSeq\":%lld,%s}", LC_PROTOCOL_VERSION, g_stateSeq + 1, g_game.seq, body);
    if (jsonLen <= 0 || (size_t)jsonLen >= sizeof(json))
        return;
    if (lc_profile_write_plugin_state(g_gameDir, json, (size_t)jsonLen)) {
        ++g_stateSeq;
        g_lastStateWriteMs = nowMs;
        memcpy(g_lastStateBody, body, (size_t)bodyLen + 1);
    }
}

static void join_path(char* out, size_t cap, const char* base, const char* suffix)
{
    const size_t len             = strlen(base);
    const int    needsSeparator  = len > 0 && base[len - 1] != '\\' && base[len - 1] != '/';
    _snprintf(out, cap, "%s%s%s", base, needsSeparator ? "\\" : "", suffix);
    out[cap - 1] = '\0';
}

/* The package installs the sounds to <plugins>\limacharlie\sounds; accept the variants TeamSpeak versions report. */
static void load_sounds(void)
{
    char base[1024];
    char candidates[3][1100];
    int  count = 0;
    if (lc_plugin_id()) {
        base[0] = '\0';
        g_ts3.getPluginPath(base, sizeof(base), lc_plugin_id());
        join_path(candidates[count++], sizeof(candidates[0]), base, "limacharlie\\sounds");
        join_path(candidates[count++], sizeof(candidates[0]), base, "plugins\\limacharlie\\sounds");
    }
    base[0] = '\0';
    g_ts3.getConfigPath(base, sizeof(base));
    join_path(candidates[count++], sizeof(candidates[0]), base, "plugins\\limacharlie\\sounds");

    for (int i = 0; i < count; ++i) {
        const int sets = lc_sounds_load(candidates[i]);
        if (sets > 0) {
            lc_logf(LC_LOG_INFO, "Loaded %d radio sound set(s) from %s", sets, candidates[i]);
            return;
        }
    }
    lc_logf(LC_LOG_WARNING, "Radio sounds not found in %s; radio beeps disabled", candidates[count - 1]);
}

static DWORD WINAPI core_main(LPVOID param)
{
    (void)param;
    char*           buf    = (char*)malloc(LC_GAME_STATE_BUFFER);
    lc_game_state* parsed = (lc_game_state*)malloc(sizeof(lc_game_state));
    if (!buf || !parsed) {
        lc_logf(LC_LOG_ERROR, "Out of memory starting the worker");
        free(buf);
        free(parsed);
        return 1;
    }

    int fastTimer = 0;
    while (g_quit == 0) {
        const unsigned long long nowMs = GetTickCount64();
        poll_game_state(buf, parsed);

        const uint64 sch = g_ts3.getCurrentServerConnectionHandlerID();
        if (sch != g_sch)
            on_connection_switched(sch);
        const int connected = is_connected(sch);
        const int inGame    = game_present();
        const int fresh     = game_fresh();

        if (inGame && !g_wasInGame)
            lc_logf(LC_LOG_INFO, "Entered game as player %d (%s)", g_game.playerId, g_game.playerName);
        if (!inGame && g_wasInGame)
            on_left_game(sch, connected);
        g_wasInGame = inGame;

        /* Said once each way, because this is the difference between "they alt-tabbed" and "they crashed" when
           reading a log afterwards. */
        if (inGame && fresh != g_wasFresh) {
            lc_logf(LC_LOG_INFO, fresh ? "Game state updating again" : "Game state paused; holding the channel");
            g_wasFresh = fresh;
        } else if (!inGame) {
            g_wasFresh = 1;
        }

        update_channel(sch, connected, inGame, nowMs);

        /* Looked up once for the whole loop: each lookup takes TeamSpeak's own locks, and this runs 200 times a
           second in game. A channel move made just above is seen next loop. */
        anyID     me          = 0;
        uint64    channel     = 0;
        const int haveChannel = connected && get_own_channel(sch, &me, &channel);

        update_handshake(sch, inGame, haveChannel, channel, nowMs);
        /* Fresh, not present: a game that stopped writing cannot be holding its transmit key. */
        update_radio_tx(sch, connected, fresh, nowMs);
        update_sound_events(inGame);
        lc_peers_expire(nowMs, LC_PEER_MAX_AGE_MS);
        lc_transmissions_expire(nowMs, LC_TRANSMISSION_MAX_AGE_MS);
        publish_voice(connected, inGame, nowMs);
        write_plugin_state(connected, inGame, haveChannel, connected && is_mic_muted(sch, nowMs), me, channel, nowMs);

        /* A 1 ms system timer is what makes the 5 ms poll actually sleep 5 ms, but it is process-wide and
           costs power everywhere, so it is only held while a game is actually writing. A game that has gone
           quiet is picked up again within one idle poll. */
        if (fresh != fastTimer) {
            if (fresh)
                timeBeginPeriod(1);
            else
                timeEndPeriod(1);
            fastTimer = fresh;
        }
        Sleep(fresh ? LC_POLL_MS : LC_IDLE_POLL_MS);
    }
    if (fastTimer)
        timeEndPeriod(1);

    if (g_txActive && is_connected(g_txSch))
        send_radio_announcement(g_txSch, 0);
    lc_audio_publish(0, NULL, 0);
    free(buf);
    free(parsed);
    return 0;
}

int lc_core_start(void)
{
    if (lc_profile_files_init() != 0) {
        lc_logf(LC_LOG_ERROR, "Could not resolve the Documents folder; game bridge disabled");
        return 1;
    }
    InitializeCriticalSection(&g_talkersLock);
    lc_peers_init();
    lc_transmissions_init();
    load_sounds();
    g_quit = 0;
    g_thread = CreateThread(NULL, 0, core_main, NULL, 0, NULL);
    if (!g_thread) {
        lc_sounds_unload();
        lc_transmissions_shutdown();
        lc_peers_shutdown();
        DeleteCriticalSection(&g_talkersLock);
        lc_logf(LC_LOG_ERROR, "Could not start the worker thread (error %lu)", GetLastError());
        return 1;
    }
    lc_logf(LC_LOG_INFO, "Lima Charlie %s started", LC_PLUGIN_VERSION);
    return 0;
}

void lc_core_stop(void)
{
    if (!g_thread)
        return;
    InterlockedExchange(&g_quit, 1);
    WaitForSingleObject(g_thread, 3000);
    CloseHandle(g_thread);
    g_thread = NULL;
    lc_sounds_unload();
    lc_transmissions_shutdown();
    lc_peers_shutdown();
    DeleteCriticalSection(&g_talkersLock);
}

void lc_core_on_talk_status(uint64 serverConnectionHandlerID, int status, anyID client)
{
    if (!g_thread || serverConnectionHandlerID != g_ts3.getCurrentServerConnectionHandlerID())
        return;
    EnterCriticalSection(&g_talkersLock);
    int index = -1;
    for (int i = 0; i < g_talkerCount; ++i) {
        if (g_talkers[i] == client) {
            index = i;
            break;
        }
    }
    if (status == STATUS_TALKING) {
        if (index < 0 && g_talkerCount < LC_MAX_TALKERS)
            g_talkers[g_talkerCount++] = client;
    } else if (index >= 0) {
        g_talkers[index] = g_talkers[--g_talkerCount];
    }
    LeaveCriticalSection(&g_talkersLock);
}

void lc_core_on_plugin_command(uint64 serverConnectionHandlerID, anyID sender, const char* command)
{
    if (!g_thread || !command || strncmp(command, LC_PEER_COMMAND_PREFIX, sizeof(LC_PEER_COMMAND_PREFIX) - 1) != 0)
        return;
    anyID me = 0;
    if (g_ts3.getClientID(serverConnectionHandlerID, &me) == ERROR_ok && me == sender)
        return;

    if (lc_transmissions_handle_command(sender, command, GetTickCount64()))
        return;

    int isNewPeer = 0;
    if (lc_peers_handle_command(sender, command, &isNewPeer) && isNewPeer)
        InterlockedExchange(&g_helloRequested, 1);
}

void lc_core_on_client_moved(uint64 serverConnectionHandlerID, anyID client, uint64 newChannelID, int visibility)
{
    if (!g_thread || serverConnectionHandlerID != g_ts3.getCurrentServerConnectionHandlerID())
        return;

    if (visibility == LEAVE_VISIBILITY || newChannelID == 0) {
        lc_peers_remove(client);
        lc_transmissions_remove_client(client);
        lc_core_on_talk_status(serverConnectionHandlerID, STATUS_NOT_TALKING, client);
        return;
    }

    /* Someone arrived in our channel, or we moved: introduce ourselves. */
    anyID  me      = 0;
    uint64 channel = 0;
    if (get_own_channel(serverConnectionHandlerID, &me, &channel) && (client == me || newChannelID == channel))
        InterlockedExchange(&g_helloRequested, 1);
}

void lc_core_on_connection_changed(uint64 serverConnectionHandlerID)
{
    (void)serverConnectionHandlerID;
    if (!g_thread)
        return;
    lc_peers_reset();
    lc_transmissions_reset();
    clear_talkers();
}
