/* Offline checks for the plugin's pure logic: spatial math, peer and radio messages, radio signal quality and
   game state parsing. Run build\lc_tests.exe; exit code is the number of failed checks. */

#include "lc_direct_voice.h"
#include "lc_game_state.h"
#include "lc_peers.h"
#include "lc_radio.h"
#include "lc_reverb.h"
#include "lc_transmissions.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                               \
        }                                                               \
    } while (0)

static const float kOrigin[3] = {0.0f, 0.0f, 0.0f};
static const float kNorth[3]  = {0.0f, 0.0f, 1.0f};
static const float kEast[3]   = {1.0f, 0.0f, 0.0f};

static float total(const float listenerPos[3], const float listenerDir[3], const float speaker[3], float range, float muffle)
{
    float left, right;
    if (!lc_direct_voice_compute(listenerPos, listenerDir, speaker, range, muffle, &left, &right))
        return 0.0f;
    return left + right;
}

static void test_direct_voice(void)
{
    float left, right;

    /* Facing north, a speaker to the east is on the right. */
    const float east10[3] = {10.0f, 0.0f, 0.0f};
    CHECK(lc_direct_voice_compute(kOrigin, kNorth, east10, 20.0f, 0.0f, &left, &right));
    CHECK(right > left);

    const float west10[3] = {-10.0f, 0.0f, 0.0f};
    CHECK(lc_direct_voice_compute(kOrigin, kNorth, west10, 20.0f, 0.0f, &left, &right));
    CHECK(left > right);

    /* Straight ahead is centred and loud. */
    const float ahead5[3] = {0.0f, 0.0f, 5.0f};
    CHECK(lc_direct_voice_compute(kOrigin, kNorth, ahead5, 20.0f, 0.0f, &left, &right));
    CHECK(fabs(left - right) < 0.01f);
    CHECK(left > 0.5f);

    /* Facing east, a speaker to the south is on the right. */
    const float south10[3] = {0.0f, 0.0f, -10.0f};
    CHECK(lc_direct_voice_compute(kOrigin, kEast, south10, 20.0f, 0.0f, &left, &right));
    CHECK(right > left);

    /* Behind is quieter than in front at the same distance. */
    const float behind5[3] = {0.0f, 0.0f, -5.0f};
    CHECK(total(kOrigin, kNorth, behind5, 20.0f, 0.0f) < total(kOrigin, kNorth, ahead5, 20.0f, 0.0f));

    /* Quieter with distance, silent at and beyond range. */
    const float ahead2[3]  = {0.0f, 0.0f, 2.0f};
    const float ahead15[3] = {0.0f, 0.0f, 15.0f};
    const float ahead19[3] = {0.0f, 0.0f, 19.5f};
    const float ahead25[3] = {0.0f, 0.0f, 25.0f};
    CHECK(total(kOrigin, kNorth, ahead2, 20.0f, 0.0f) > total(kOrigin, kNorth, ahead15, 20.0f, 0.0f));
    CHECK(total(kOrigin, kNorth, ahead15, 20.0f, 0.0f) > total(kOrigin, kNorth, ahead19, 20.0f, 0.0f));
    CHECK(!lc_direct_voice_compute(kOrigin, kNorth, ahead25, 20.0f, 0.0f, &left, &right));

    /* A shout carries further than normal speech. */
    CHECK(lc_direct_voice_compute(kOrigin, kNorth, ahead25, 60.0f, 0.0f, &left, &right));

    /* Muffling reduces volume; gains never exceed 1. */
    CHECK(total(kOrigin, kNorth, ahead5, 20.0f, 1.0f) < total(kOrigin, kNorth, ahead5, 20.0f, 0.0f));
    const float right1[3] = {1.0f, 0.0f, 0.0f};
    CHECK(lc_direct_voice_compute(kOrigin, kNorth, right1, 20.0f, 0.0f, &left, &right));
    CHECK(left <= 1.0f && right <= 1.0f);
}

static void test_hello(void)
{
    char   command[128];
    size_t len = lc_peers_format_hello(command, sizeof(command), "1789377566-1-2-3", 7, 60.0f, "1.0.0", "1.2.3");
    CHECK(len > 0);
    CHECK(strcmp(command, "LC1|HELLO|1789377566-1-2-3|7|60.0|1.0.0|1.2.3") == 0);

    lc_peers_init();
    int isNew = 0;
    CHECK(lc_peers_handle_command(42, command, &isNew));
    CHECK(isNew);
    CHECK(lc_peers_player_id(42, "1789377566-1-2-3") == 7);
    CHECK(lc_peers_player_id(42, "other-session") == -1);

    lc_peer_info peers[4];
    CHECK(lc_peers_list("1789377566-1-2-3", peers, 4) == 1);
    CHECK(fabs(peers[0].voiceRange - 60.0f) < 0.01f);
    /* The versions ride along on the HELLO, for the TeamSpeak info panel. */
    CHECK(strcmp(peers[0].pluginVersion, "1.0.0") == 0);
    CHECK(strcmp(peers[0].modVersion, "1.2.3") == 0);
    lc_peer_info found;
    CHECK(lc_peers_find(42, &found) && found.playerId == 7);
    CHECK(!lc_peers_find(9999, &found));

    /* A range change alone is not a new peer; a HELLO without a range falls back to the default. */
    lc_peers_format_hello(command, sizeof(command), "1789377566-1-2-3", 7, 5.0f, "1.0.0", "1.2.3");
    CHECK(lc_peers_handle_command(42, command, &isNew));
    CHECK(!isNew);
    /* A HELLO from a build that sends no versions is still valid; the fields just stay empty. */
    CHECK(lc_peers_handle_command(43, "LC1|HELLO|1789377566-1-2-3|8", &isNew));
    CHECK(lc_peers_find(43, &found) && found.pluginVersion[0] == 0 && found.modVersion[0] == 0);
    CHECK(lc_peers_list("1789377566-1-2-3", peers, 4) == 2);
    CHECK(fabs(peers[1].voiceRange - LC_DEFAULT_VOICE_RANGE) < 0.01f || fabs(peers[0].voiceRange - LC_DEFAULT_VOICE_RANGE) < 0.01f);

    CHECK(!lc_peers_handle_command(44, "LC1|HELLO||3", &isNew));
    CHECK(!lc_peers_handle_command(44, "LC1|HELLO|token|0", &isNew));
    CHECK(!lc_peers_handle_command(44, "SOMETHING ELSE", &isNew));
    /* Radio announcements are not HELLOs. */
    CHECK(!lc_peers_handle_command(44, "LC1|RTX|token|3|1|45000|1500|0.0|0.0|0.0|US", &isNew));
    lc_peers_shutdown();
}

static void test_radio_announcement(void)
{
    char        command[256];
    const float pos[3] = {1.5f, -2.0f, 3000.25f};
    CHECK(lc_radio_format_announcement(command, sizeof(command), "tok-1", 7, 1, 45000, 1500.0f, pos, "US|X") > 0);
    CHECK(strcmp(command, "LC1|RTX|tok-1|7|1|45000|1500|1.5|-2.0|3000.2|US_X") == 0 || strcmp(command, "LC1|RTX|tok-1|7|1|45000|1500|1.5|-2.0|3000.3|US_X") == 0);

    lc_radio_announcement parsed;
    CHECK(lc_radio_parse_announcement(command, &parsed));
    CHECK(strcmp(parsed.token, "tok-1") == 0);
    CHECK(parsed.playerId == 7 && parsed.on && parsed.frequency == 45000);
    CHECK(fabs(parsed.range - 1500.0f) < 0.5f);
    CHECK(fabs(parsed.pos[0] - 1.5f) < 0.01f && fabs(parsed.pos[1] + 2.0f) < 0.01f && fabs(parsed.pos[2] - 3000.25f) < 0.1f);
    CHECK(strcmp(parsed.key, "US_X") == 0);

    /* A stop without an encryption key is valid. */
    CHECK(lc_radio_parse_announcement("LC1|RTX|tok|7|0|45000|1500|1|2|3|", &parsed));
    CHECK(!parsed.on && parsed.key[0] == '\0');

    CHECK(!lc_radio_parse_announcement("LC1|RTX|tok|0|1|45000|1500|1|2|3|US", &parsed));
    CHECK(!lc_radio_parse_announcement("LC1|RTX|tok|7|1|45000", &parsed));
    CHECK(!lc_radio_parse_announcement("LC1|RTX|tok|7|1|0|1500|1|2|3|US", &parsed));
    CHECK(!lc_radio_parse_announcement("LC1|RTX||7|1|45000|1500|1|2|3|US", &parsed));
    CHECK(!lc_radio_parse_announcement("LC1|HELLO|tok|7|20.0", &parsed));
}

static void test_radio_quality(void)
{
    const float clean = LC_RADIO_CLEAN_FRACTION;

    /* Clean up close, garbling progressively (never louder or quieter, only worse) out to the edge of range. */
    CHECK(lc_radio_quality(100.0f, 0.0f, 1000.0f, clean) == 1.0f);
    CHECK(lc_radio_quality(350.0f, 0.0f, 1000.0f, clean) == 1.0f);
    const float mid  = lc_radio_quality(600.0f, 0.0f, 1000.0f, clean);
    const float far_ = lc_radio_quality(900.0f, 0.0f, 1000.0f, clean);
    CHECK(mid < 1.0f && mid > far_ && far_ > 0.0f);
    CHECK(lc_radio_quality(999.0f, 0.0f, 1000.0f, clean) >= LC_RADIO_EDGE_QUALITY - 0.001f);
    CHECK(lc_radio_quality(1000.0f, 0.0f, 1000.0f, clean) == 0.0f);
    CHECK(lc_radio_quality(5000.0f, 0.0f, 1000.0f, clean) == 0.0f);
    CHECK(lc_radio_quality(10.0f, 0.0f, 0.0f, clean) == 0.0f);

    /* Terrain adds distance the way TFAR does: d + h*7 + h*7*(d/2000). */
    CHECK(fabs(lc_radio_effective_distance(1000.0f, 0.0f) - 1000.0f) < 0.01f);
    CHECK(fabs(lc_radio_effective_distance(1000.0f, 10.0f) - 1105.0f) < 0.01f);
    CHECK(lc_radio_quality(500.0f, 20.0f, 1000.0f, clean) < lc_radio_quality(500.0f, 0.0f, 1000.0f, clean));
    /* A big ridge cuts a link that would otherwise be clean. */
    CHECK(lc_radio_quality(300.0f, 0.0f, 1000.0f, clean) == 1.0f);
    CHECK(lc_radio_quality(300.0f, 120.0f, 1000.0f, clean) == 0.0f);

    /* Quality falls in a straight line across the garbled band: equal steps of distance, equal steps of
       quality. Halfway through the band is halfway between clean and the edge quality. */
    const float band  = 1.0f - clean;
    const float halfway = lc_radio_quality((clean + band * 0.5f) * 1000.0f, 0.0f, 1000.0f, clean);
    const float quarter = lc_radio_quality((clean + band * 0.25f) * 1000.0f, 0.0f, 1000.0f, clean);
    const float threeQ  = lc_radio_quality((clean + band * 0.75f) * 1000.0f, 0.0f, 1000.0f, clean);
    CHECK(fabs(halfway - (1.0f + LC_RADIO_EDGE_QUALITY) * 0.5f) < 0.01f);
    CHECK(fabs((quarter - halfway) - (halfway - threeQ)) < 0.01f);

    /* Range ratio is what the beep cutoff is measured against: 1 exactly at the edge. */
    CHECK(fabs(lc_radio_range_ratio(500.0f, 0.0f, 1000.0f) - 0.5f) < 0.001f);
    CHECK(fabs(lc_radio_range_ratio(1000.0f, 0.0f, 1000.0f) - 1.0f) < 0.001f);
    CHECK(lc_radio_range_ratio(500.0f, 20.0f, 1000.0f) > 0.5f);
    CHECK(lc_radio_range_ratio(10.0f, 0.0f, 0.0f) > 1.0f);

    /* The clean fraction only moves where garbling starts; range itself is untouched. */
    CHECK(lc_radio_quality(500.0f, 0.0f, 1000.0f, 0.8f) == 1.0f);
    CHECK(lc_radio_quality(500.0f, 0.0f, 1000.0f, 0.1f) < 1.0f);
    CHECK(lc_radio_quality(500.0f, 0.0f, 1000.0f, 0.1f) < lc_radio_quality(500.0f, 0.0f, 1000.0f, clean));
    CHECK(lc_radio_quality(999.0f, 0.0f, 1000.0f, 0.8f) > 0.0f);
    CHECK(lc_radio_quality(1000.0f, 0.0f, 1000.0f, 0.8f) == 0.0f);
    /* A clean fraction of 1 would make range a cliff, so it is capped short of the edge. */
    CHECK(lc_radio_quality(999.0f, 0.0f, 1000.0f, 1.0f) < 1.0f);
    /* Nonsense falls back to the default. */
    CHECK(lc_radio_quality(350.0f, 0.0f, 1000.0f, -1.0f) == 1.0f);
    CHECK(lc_radio_quality(600.0f, 0.0f, 1000.0f, -1.0f) == mid);
}

static void test_transmissions(void)
{
    lc_transmissions_init();
    lc_transmission list[4];

    CHECK(lc_transmissions_handle_command(42, "LC1|RTX|tok|7|1|45000|1500|1|2|3|US", 1000));
    CHECK(lc_transmissions_list("tok", list, 4) == 1);
    CHECK(list[0].client == 42 && list[0].info.playerId == 7 && list[0].startMs == 1000);
    CHECK(lc_transmissions_list("other", list, 4) == 0);

    /* Refreshes keep the start time; retuning restarts it. */
    CHECK(lc_transmissions_handle_command(42, "LC1|RTX|tok|7|1|45000|1500|50|2|3|US", 2000));
    CHECK(lc_transmissions_list("tok", list, 4) == 1 && list[0].startMs == 1000 && fabs(list[0].info.pos[0] - 50.0f) < 0.01f);
    CHECK(lc_transmissions_handle_command(42, "LC1|RTX|tok|7|1|46000|1500|50|2|3|US", 2500));
    CHECK(lc_transmissions_list("tok", list, 4) == 1 && list[0].startMs == 2500);

    /* Stop, expiry and leaving the channel remove it. */
    CHECK(lc_transmissions_handle_command(42, "LC1|RTX|tok|7|0|46000|1500|50|2|3|US", 3000));
    CHECK(lc_transmissions_list("tok", list, 4) == 0);
    CHECK(lc_transmissions_handle_command(43, "LC1|RTX|tok|8|1|45000|1500|1|2|3|US", 3000));
    lc_transmissions_expire(5000, 3500);
    CHECK(lc_transmissions_list("tok", list, 4) == 1);
    lc_transmissions_expire(7000, 3500);
    CHECK(lc_transmissions_list("tok", list, 4) == 0);
    CHECK(lc_transmissions_handle_command(44, "LC1|RTX|tok|9|1|45000|1500|1|2|3|US", 8000));
    lc_transmissions_remove_client(44);
    CHECK(lc_transmissions_list("tok", list, 4) == 0);

    CHECK(!lc_transmissions_handle_command(45, "LC1|HELLO|tok|9|20.0", 8000));
    lc_transmissions_shutdown();
}

static void test_game_state(void)
{
    static const char json[] =
        "{\"v\":8,\"seq\":12,\"inGame\":true,"
        "\"session\":{\"token\":\"abc\",\"playerId\":3,\"playerName\":\"A \\\"quoted\\\" name\",\"tsServer\":\"\",\"tsChannel\":\"Squad 1\",\"tsChannelPassword\":\"pw\"},"
        "\"self\":{\"alive\":true,\"pos\":[1.5,2,3],\"dir\":[0,0,1],\"tx\":2,\"txFrequency\":45000,\"txRadio\":\"77:1\",\"voiceRange\":5,\"cleanFraction\":0.6,\"beepFraction\":0.8,\"unlimitedRx\":true,\"roomVolume\":96,"
        "\"radios\":[{\"id\":\"77:1\",\"freq\":45000,\"range\":1500,\"key\":\"US\",\"rx\":true,\"ear\":1,\"volume\":0.4,\"beep\":\"tfar_sw\",\"halfDuplex\":1},"
        "{\"id\":\"78:1\",\"freq\":60000,\"range\":16000,\"key\":\"US\",\"rx\":false,\"ear\":9,\"volume\":7,\"beep\":\"acre\"}],"
        "\"sounds\":[{\"seq\":4,\"set\":\"acre\",\"name\":\"local_start\",\"ear\":2,\"volume\":0.5},{\"seq\":5,\"set\":\"ui\",\"name\":\"deny\"}]},"
        "\"players\":[{\"id\":4,\"alive\":true,\"pos\":[4,5,6],\"dir\":[1,0,0],\"muffle\":0.6,\"room\":0.35},{\"id\":5,\"alive\":false,\"pos\":[0,0,0],\"dir\":[0,0,1]}],"
        "\"links\":[{\"id\":4,\"clearance\":35}]}";
    static lc_game_state state;
    CHECK(lc_game_state_parse(json, &state));
    CHECK(state.inGame && state.seq == 12);
    CHECK(strcmp(state.token, "abc") == 0 && state.playerId == 3);
    CHECK(strcmp(state.playerName, "A \"quoted\" name") == 0);
    CHECK(strcmp(state.tsChannel, "Squad 1") == 0);
    CHECK(state.tx == LC_TX_CHANNEL && fabs(state.voiceRange - 5.0f) < 0.01f);
    CHECK(fabs(state.cleanFraction - 0.6f) < 0.01f);
    CHECK(fabs(state.beepFraction - 0.8f) < 0.01f);
    /* Set only while a Game Master has the editor open; absent means the normal range rules apply. */
    CHECK(state.unlimitedRx);
    CHECK(fabs(state.roomVolume - 96.0f) < 0.01f);
    CHECK(strcmp(state.txRadio, "77:1") == 0 && state.txFrequency == 45000);
    CHECK(fabs(state.pos[0] - 1.5f) < 0.01f);
    CHECK(state.playerCount == 2);
    CHECK(state.players[0].id == 4 && fabs(state.players[0].muffle - 0.6f) < 0.01f);
    CHECK(!state.players[1].alive && state.players[1].muffle == 0.0f);

    CHECK(state.radioCount == 2);
    CHECK(strcmp(state.radios[0].id, "77:1") == 0 && state.radios[0].frequency == 45000 && fabs(state.radios[0].range - 1500.0f) < 0.1f);
    CHECK(strcmp(state.radios[0].key, "US") == 0 && state.radios[0].rx && state.radios[0].ear == LC_EAR_LEFT);
    CHECK(strcmp(state.radios[0].beep, "tfar_sw") == 0);
    /* Duplex mode comes per radio; a radio that omits it is full duplex. */
    CHECK(state.radios[0].halfDuplex);
    CHECK(!state.radios[1].halfDuplex);
    CHECK(fabs(state.radios[0].volume - 0.4f) < 0.01f);
    CHECK(!state.radios[1].rx && state.radios[1].ear == LC_EAR_BOTH && state.radios[1].volume == 1.0f);
    CHECK(state.soundCount == 2);
    CHECK(state.sounds[0].seq == 4 && strcmp(state.sounds[0].set, "acre") == 0 && strcmp(state.sounds[0].name, "local_start") == 0);
    CHECK(state.sounds[0].ear == LC_EAR_RIGHT && fabs(state.sounds[0].volume - 0.5f) < 0.01f);
    CHECK(state.sounds[1].seq == 5 && strcmp(state.sounds[1].name, "deny") == 0 && state.sounds[1].ear == LC_EAR_BOTH && state.sounds[1].volume == 1.0f);
    /* The room share scales the reverb send; a player entry without one is treated as in the room. */
    CHECK(fabs(state.players[0].room - 0.35f) < 0.01f);
    CHECK(state.players[1].room == 1.0f);
    CHECK(state.linkCount == 1 && state.links[0].playerId == 4 && fabs(state.links[0].clearance - 35.0f) < 0.01f);

    /* Half-written files and other protocol versions are rejected. */
    CHECK(!lc_game_state_parse("{\"v\":8,\"seq\":12,\"inGa", &state));
    CHECK(!lc_game_state_parse("{\"v\":1,\"seq\":1}", &state));
    CHECK(!lc_game_state_parse("{\"v\":7,\"seq\":1}", &state));
    CHECK(!lc_game_state_parse("{\"v\":9,\"seq\":1}", &state));
    CHECK(lc_game_state_parse("{\"v\":8,\"seq\":13,\"inGame\":false}", &state) && !state.inGame && state.radioCount == 0);
    /* An omitted beep range falls back to the default rather than silencing every beep. */
    CHECK(fabs(state.beepFraction - LC_RADIO_BEEP_FRACTION) < 0.01f);
    CHECK(!state.unlimitedRx);
    /* Outdoors, and older states that never carried it, read as no room at all. */
    CHECK(state.roomVolume == 0.0f);
}

static void test_reverb(void)
{
    float wet, decay;

    /* Outdoors, and spaces too small to ring, have no tail at all. */
    lc_reverb_room_params(0.0f, &wet, &decay);
    CHECK(wet == 0.0f);
    lc_reverb_room_params(10.0f, &wet, &decay);
    CHECK(wet == 0.0f);

    /* A room the size of a shed is wet but short; a hangar is wetter and longer. */
    float smallWet, smallDecay, mediumWet, mediumDecay, largeWet, largeDecay;
    lc_reverb_room_params(100.0f, &smallWet, &smallDecay);
    lc_reverb_room_params(800.0f, &mediumWet, &mediumDecay);
    lc_reverb_room_params(20000.0f, &largeWet, &largeDecay);
    CHECK(smallWet > 0.0f && smallWet < mediumWet && mediumWet < largeWet);
    CHECK(smallDecay < mediumDecay && mediumDecay < largeDecay);
    /* However large the room, the tail must stay below unity or it would never die away. */
    CHECK(largeDecay < 1.0f && largeWet < 0.5f);

    /* An impulse into a room decays towards silence and never turns into a NaN or a rising howl. */
    lc_reverb_set_room(800.0f);
    short  frames[960 * 2];
    float  impulse[960];
    double first = 0, last = 0;
    for (int i = 0; i < 960; ++i)
        impulse[i] = 0.0f;

    impulse[0] = 0.8f;
    for (int period = 0; period < 40; ++period) {
        memset(frames, 0, sizeof(frames));
        lc_reverb_send(impulse, 960);
        lc_reverb_mix(frames, 960, 2, NULL, NULL);

        double energy = 0;
        for (int i = 0; i < 960 * 2; ++i) {
            CHECK(frames[i] == frames[i]); /* NaN would fail its own equality */
            energy += (double)frames[i] * (double)frames[i];
        }

        if (period == 1)
            first = energy;
        if (period == 39)
            last = energy;

        /* Only the first period carries the impulse; the rest is tail. */
        impulse[0] = 0.0f;
    }

    CHECK(first > 0);
    CHECK(last < first);

    /* Stepping outdoors drops the tail rather than leaving it ringing. */
    lc_reverb_set_room(0.0f);
    memset(frames, 0, sizeof(frames));
    lc_reverb_mix(frames, 960, 2, NULL, NULL);
    for (int i = 0; i < 960 * 2; ++i)
        CHECK(frames[i] == 0);
}

int main(void)
{
    test_direct_voice();
    test_hello();
    test_radio_announcement();
    test_radio_quality();
    test_transmissions();
    test_game_state();
    test_reverb();
    printf(g_failures ? "%d check(s) failed\n" : "All checks passed\n", g_failures);
    return g_failures;
}
