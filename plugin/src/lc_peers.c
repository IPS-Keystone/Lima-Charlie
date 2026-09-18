#include "lc_peers.h"

#include "lc_direct_voice.h"
#include "lc_game_state.h"
#include "lc_log.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LC_MAX_PEERS 256
#define LC_MIN_VOICE_RANGE 1.0f
#define LC_MAX_VOICE_RANGE 200.0f

typedef struct {
    anyID              client;
    int                playerId;
    float              voiceRange;
    char               token[LC_TOKEN_CAP];
    char               pluginVersion[32];
    char               modVersion[32];
    unsigned long long lastSeenMs;
} lc_peer;

static CRITICAL_SECTION g_lock;
static int              g_initialized;
static lc_peer         g_peers[LC_MAX_PEERS];
static int              g_count;

static int find_index(anyID client)
{
    for (int i = 0; i < g_count; ++i) {
        if (g_peers[i].client == client)
            return i;
    }
    return -1;
}

static void remove_index(int index)
{
    g_peers[index] = g_peers[--g_count];
}

void lc_peers_init(void)
{
    InitializeCriticalSection(&g_lock);
    g_count       = 0;
    g_initialized = 1;
}

void lc_peers_shutdown(void)
{
    if (!g_initialized)
        return;
    g_initialized = 0;
    DeleteCriticalSection(&g_lock);
}

void lc_peers_reset(void)
{
    EnterCriticalSection(&g_lock);
    g_count = 0;
    LeaveCriticalSection(&g_lock);
}

/* Copies the field starting at the separator at, up to the next separator or the end. */
static void copy_field(const char* at, char* out, size_t cap)
{
    out[0] = '\0';
    if (!at || *at != '|')
        return;
    ++at;
    size_t i = 0;
    while (at[i] && at[i] != '|' && i + 1 < cap) {
        out[i] = at[i];
        ++i;
    }
    out[i] = '\0';
}

int lc_peers_handle_command(anyID sender, const char* command, int* isNewPeer)
{
    static const char prefix[] = LC_PEER_COMMAND_PREFIX "HELLO|";
    *isNewPeer                 = 0;
    if (strncmp(command, prefix, sizeof(prefix) - 1) != 0)
        return 0;

    const char*  token    = command + sizeof(prefix) - 1;
    const char*  sep      = strchr(token, '|');
    const size_t tokenLen = sep ? (size_t)(sep - token) : 0;
    if (tokenLen == 0 || tokenLen >= LC_TOKEN_CAP)
        return 0;
    char*      end;
    const long playerId = strtol(sep + 1, &end, 10);
    if (end == sep + 1 || playerId <= 0)
        return 0;

    float voiceRange = LC_DEFAULT_VOICE_RANGE;
    char  pluginVersion[32];
    char  modVersion[32];
    pluginVersion[0] = '\0';
    modVersion[0]    = '\0';
    if (*end == '|') {
        char*        rangeEnd;
        const double parsed = strtod(end + 1, &rangeEnd);
        if (rangeEnd != end + 1 && parsed >= LC_MIN_VOICE_RANGE && parsed <= LC_MAX_VOICE_RANGE)
            voiceRange = (float)parsed;
        /* Older builds stop here; the two version fields are optional. */
        copy_field(rangeEnd, pluginVersion, sizeof(pluginVersion));
        copy_field(strchr(rangeEnd + (*rangeEnd == '|' ? 1 : 0), '|'), modVersion, sizeof(modVersion));
    }

    EnterCriticalSection(&g_lock);
    int index = find_index(sender);
    if (index < 0 && g_count < LC_MAX_PEERS) {
        index = g_count++;
        memset(&g_peers[index], 0, sizeof(g_peers[index]));
        g_peers[index].client = sender;
        *isNewPeer            = 1;
    }
    if (index >= 0) {
        lc_peer* p = &g_peers[index];
        if (p->playerId != (int)playerId || strlen(p->token) != tokenLen || strncmp(p->token, token, tokenLen) != 0) {
            *isNewPeer = 1;
            lc_logf(LC_LOG_INFO, "Peer TS client %u is Reforger player %ld", (unsigned)sender, playerId);
        }
        p->playerId   = (int)playerId;
        p->voiceRange = voiceRange;
        if (pluginVersion[0])
            strncpy(p->pluginVersion, pluginVersion, sizeof(p->pluginVersion) - 1);
        if (modVersion[0])
            strncpy(p->modVersion, modVersion, sizeof(p->modVersion) - 1);
        memcpy(p->token, token, tokenLen);
        p->token[tokenLen] = '\0';
        p->lastSeenMs      = GetTickCount64();
    }
    LeaveCriticalSection(&g_lock);
    return 1;
}

int lc_peers_player_id(anyID client, const char* token)
{
    int playerId = -1;
    EnterCriticalSection(&g_lock);
    const int index = find_index(client);
    if (index >= 0 && strcmp(g_peers[index].token, token) == 0)
        playerId = g_peers[index].playerId;
    LeaveCriticalSection(&g_lock);
    return playerId;
}

int lc_peers_list(const char* token, lc_peer_info* out, int max)
{
    int count = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_count && count < max; ++i) {
        if (strcmp(g_peers[i].token, token) != 0)
            continue;
        out[count].client     = g_peers[i].client;
        out[count].playerId   = g_peers[i].playerId;
        out[count].voiceRange = g_peers[i].voiceRange;
        strncpy(out[count].pluginVersion, g_peers[i].pluginVersion, sizeof(out[count].pluginVersion) - 1);
        out[count].pluginVersion[sizeof(out[count].pluginVersion) - 1] = '\0';
        strncpy(out[count].modVersion, g_peers[i].modVersion, sizeof(out[count].modVersion) - 1);
        out[count].modVersion[sizeof(out[count].modVersion) - 1] = '\0';
        ++count;
    }
    LeaveCriticalSection(&g_lock);
    return count;
}

int lc_peers_find(anyID client, lc_peer_info* out)
{
    int found = 0;
    EnterCriticalSection(&g_lock);
    const int index = find_index(client);
    if (index >= 0) {
        const lc_peer* p = &g_peers[index];
        out->client      = p->client;
        out->playerId    = p->playerId;
        out->voiceRange  = p->voiceRange;
        strncpy(out->pluginVersion, p->pluginVersion, sizeof(out->pluginVersion) - 1);
        out->pluginVersion[sizeof(out->pluginVersion) - 1] = '\0';
        strncpy(out->modVersion, p->modVersion, sizeof(out->modVersion) - 1);
        out->modVersion[sizeof(out->modVersion) - 1] = '\0';
        found = 1;
    }
    LeaveCriticalSection(&g_lock);
    return found;
}

int lc_peers_count(const char* token)
{
    int count = 0;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_count; ++i) {
        if (strcmp(g_peers[i].token, token) == 0)
            ++count;
    }
    LeaveCriticalSection(&g_lock);
    return count;
}

void lc_peers_remove(anyID client)
{
    EnterCriticalSection(&g_lock);
    const int index = find_index(client);
    if (index >= 0)
        remove_index(index);
    LeaveCriticalSection(&g_lock);
}

void lc_peers_expire(unsigned long long nowMs, unsigned long long maxAgeMs)
{
    EnterCriticalSection(&g_lock);
    for (int i = g_count - 1; i >= 0; --i) {
        if (nowMs - g_peers[i].lastSeenMs > maxAgeMs)
            remove_index(i);
    }
    LeaveCriticalSection(&g_lock);
}

size_t lc_peers_format_hello(char* out, size_t cap, const char* token, int playerId, float voiceRange,
                             const char* pluginVersion, const char* modVersion)
{
    const int n = _snprintf(out, cap, LC_PEER_COMMAND_PREFIX "HELLO|%s|%d|%.1f|%s|%s", token, playerId, voiceRange,
                            pluginVersion ? pluginVersion : "", modVersion ? modVersion : "");
    if (n < 0 || (size_t)n >= cap) {
        if (cap)
            out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}
