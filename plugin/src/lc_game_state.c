#include "lc_game_state.h"

#include "cJSON.h"
#include "lc_direct_voice.h"
#include "lc_radio.h"
#include "lc_version.h"

#include <string.h>

static void copy_string(const cJSON* obj, const char* name, char* dst, size_t cap)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, name);
    dst[0]            = '\0';
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, cap - 1);
        dst[cap - 1] = '\0';
    }
}

static int get_int(const cJSON* obj, const char* name, int fallback)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, name);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static float get_float(const cJSON* obj, const char* name, float fallback)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, name);
    return cJSON_IsNumber(item) ? (float)item->valuedouble : fallback;
}

static int get_bool(const cJSON* obj, const char* name)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (cJSON_IsBool(item))
        return cJSON_IsTrue(item) ? 1 : 0;
    if (cJSON_IsNumber(item))
        return item->valuedouble != 0.0;
    return 0;
}

static void get_vec3(const cJSON* obj, const char* name, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    const cJSON* item        = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) < 3)
        return;
    for (int i = 0; i < 3; ++i) {
        const cJSON* element = cJSON_GetArrayItem(item, i);
        if (cJSON_IsNumber(element))
            out[i] = (float)element->valuedouble;
    }
}

static int get_ear(const cJSON* obj, const char* name)
{
    const int ear = get_int(obj, name, LC_EAR_BOTH);
    return ear == LC_EAR_LEFT || ear == LC_EAR_RIGHT ? ear : LC_EAR_BOTH;
}

static float get_volume(const cJSON* obj, const char* name)
{
    const float volume = get_float(obj, name, 1.0f);
    return volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
}

static void parse_sounds(const cJSON* self, lc_game_state* out)
{
    const cJSON* sounds = cJSON_GetObjectItemCaseSensitive(self, "sounds");
    const cJSON* sound;
    cJSON_ArrayForEach(sound, sounds)
    {
        if (out->soundCount >= LC_GAME_STATE_MAX_SOUNDS)
            break;
        if (!cJSON_IsObject(sound))
            continue;
        lc_sound_event* s = &out->sounds[out->soundCount++];
        s->seq             = get_int(sound, "seq", 0);
        copy_string(sound, "set", s->set, sizeof(s->set));
        copy_string(sound, "name", s->name, sizeof(s->name));
        s->ear    = get_ear(sound, "ear");
        s->volume = get_volume(sound, "volume");
    }
}

static void parse_radios(const cJSON* self, lc_game_state* out)
{
    const cJSON* radios = cJSON_GetObjectItemCaseSensitive(self, "radios");
    const cJSON* radio;
    cJSON_ArrayForEach(radio, radios)
    {
        if (out->radioCount >= LC_GAME_STATE_MAX_RADIOS)
            break;
        if (!cJSON_IsObject(radio))
            continue;
        lc_radio_state* r = &out->radios[out->radioCount++];
        copy_string(radio, "id", r->id, sizeof(r->id));
        r->frequency = get_int(radio, "freq", 0);
        r->range     = get_float(radio, "range", 0.0f);
        copy_string(radio, "key", r->key, sizeof(r->key));
        r->rx  = get_bool(radio, "rx");
        r->ear    = get_ear(radio, "ear");
        r->volume = get_volume(radio, "volume");
        copy_string(radio, "beep", r->beep, sizeof(r->beep));
        r->halfDuplex = get_bool(radio, "halfDuplex");
    }
}

int lc_game_state_parse(const char* json, lc_game_state* out)
{
    cJSON* root = cJSON_Parse(json);
    if (!root)
        return 0;

    int ok = 0;
    if (get_int(root, "v", 0) == LC_PROTOCOL_VERSION) {
        memset(out, 0, sizeof(*out));

        const cJSON* seq = cJSON_GetObjectItemCaseSensitive(root, "seq");
        out->seq         = cJSON_IsNumber(seq) ? (long long)seq->valuedouble : 0;
        out->inGame      = get_bool(root, "inGame");

        const cJSON* session = cJSON_GetObjectItemCaseSensitive(root, "session");
        copy_string(session, "token", out->token, sizeof(out->token));
        out->playerId = get_int(session, "playerId", 0);
        copy_string(session, "playerName", out->playerName, sizeof(out->playerName));
        copy_string(session, "tsServer", out->tsServer, sizeof(out->tsServer));
        copy_string(session, "tsChannel", out->tsChannel, sizeof(out->tsChannel));
        copy_string(session, "tsChannelPassword", out->tsChannelPassword, sizeof(out->tsChannelPassword));
        copy_string(session, "modVersion", out->modVersion, sizeof(out->modVersion));

        const cJSON* self = cJSON_GetObjectItemCaseSensitive(root, "self");
        out->alive        = get_bool(self, "alive");
        get_vec3(self, "pos", out->pos);
        get_vec3(self, "dir", out->dir);
        out->tx          = get_int(self, "tx", LC_TX_NONE);
        out->txFrequency = get_int(self, "txFrequency", 0);
        copy_string(self, "txRadio", out->txRadio, sizeof(out->txRadio));
        out->voiceRange    = get_float(self, "voiceRange", LC_DEFAULT_VOICE_RANGE);
        out->cleanFraction = get_float(self, "cleanFraction", LC_RADIO_CLEAN_FRACTION);
        out->beepFraction  = get_float(self, "beepFraction", LC_RADIO_BEEP_FRACTION);
        out->unlimitedRx   = get_bool(self, "unlimitedRx");
        parse_radios(self, out);
        parse_sounds(self, out);

        const cJSON* players = cJSON_GetObjectItemCaseSensitive(root, "players");
        const cJSON* player;
        cJSON_ArrayForEach(player, players)
        {
            if (out->playerCount >= LC_GAME_STATE_MAX_PLAYERS)
                break;
            if (!cJSON_IsObject(player))
                continue;
            lc_player_state* p = &out->players[out->playerCount++];
            p->id               = get_int(player, "id", 0);
            p->alive            = get_bool(player, "alive");
            get_vec3(player, "pos", p->pos);
            p->muffle = get_float(player, "muffle", 0.0f);
        }

        const cJSON* links = cJSON_GetObjectItemCaseSensitive(root, "links");
        const cJSON* link;
        cJSON_ArrayForEach(link, links)
        {
            if (out->linkCount >= LC_GAME_STATE_MAX_LINKS)
                break;
            if (!cJSON_IsObject(link))
                continue;
            lc_link_state* l = &out->links[out->linkCount++];
            l->playerId       = get_int(link, "id", 0);
            l->clearance      = get_float(link, "clearance", 0.0f);
        }
        ok = 1;
    }

    cJSON_Delete(root);
    return ok;
}
