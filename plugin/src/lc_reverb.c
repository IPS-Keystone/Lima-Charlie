#include "lc_reverb.h"

#include "lc_audio.h"

#include <windows.h>

#include <math.h>
#include <string.h>

/* A Schroeder reverb: four parallel combs for the tail, two series allpasses to break up its comb ringing.
   The right channel uses slightly longer delays than the left so the tail is not identical in both ears. */
#define LC_COMBS 4
#define LC_ALLPASSES 2
/* Comb delays at 48 kHz, prime-ish so their echoes do not line up */
static const int kCombDelay[LC_COMBS] = {1237, 1381, 1481, 1601};
static const int kAllpassDelay[LC_ALLPASSES] = {601, 431};
/* Frames added to every right-channel delay */
#define LC_STEREO_OFFSET 23
#define LC_MAX_DELAY (1601 + LC_STEREO_OFFSET + 1)
/* One period of send at 48 kHz is well under this */
#define LC_SEND_CAP 8192
/* Damping in the comb feedback: a room's tail loses its top end first */
#define LC_DAMPING 0.28f
#define LC_ALLPASS_GAIN 0.5f

/* Room volumes in cubic metres at which the tail reaches its small, medium and large shape */
#define LC_ROOM_MIN_M3 25.0f
#define LC_ROOM_SMALL_M3 150.0f
#define LC_ROOM_LARGE_M3 4000.0f

#define LC_WET_SMALL 0.10f
#define LC_WET_LARGE 0.26f
#define LC_DECAY_SMALL 0.62f
#define LC_DECAY_LARGE 0.87f

typedef struct {
    float buffer[LC_MAX_DELAY];
    int   length;
    int   index;
    float store;
} lc_comb;

typedef struct {
    float buffer[LC_MAX_DELAY];
    int   length;
    int   index;
} lc_allpass;

typedef struct {
    lc_comb     combs[LC_COMBS];
    lc_allpass allpasses[LC_ALLPASSES];
} lc_reverb_channel;

static lc_reverb_channel g_channels[2];
static int               g_ready;

/* Written by every per-client edit in a period, drained by the mixed edit that follows. */
static float g_send[LC_SEND_CAP];
static int   g_sendCount;

/* Room volume from the game. Written by the worker, read by the audio thread; a stale read is one buffer
   of slightly wrong tail, which is not worth a lock. */
static volatile LONG g_roomVolumeBits;

static float bits_to_float(LONG bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void lc_reverb_set_room(float volumeM3)
{
    LONG bits;
    if (volumeM3 < 0.0f)
        volumeM3 = 0.0f;

    memcpy(&bits, &volumeM3, sizeof(bits));
    InterlockedExchange(&g_roomVolumeBits, bits);
}

void lc_reverb_room_params(float volumeM3, float* wet, float* decay)
{
    *wet   = 0.0f;
    *decay = LC_DECAY_SMALL;
    if (volumeM3 < LC_ROOM_MIN_M3)
        return; /* outdoors, or a space too small to ring */

    /* Between the small and large room sizes both the amount and the length of the tail grow with the
       logarithm of the volume, so a garage and a hangar are clearly different but a cupboard and a bedroom
       are not. */
    float t = 0.0f;
    if (volumeM3 > LC_ROOM_SMALL_M3) {
        const float span = (float)log(LC_ROOM_LARGE_M3 / LC_ROOM_SMALL_M3);
        t                = (float)log(volumeM3 / LC_ROOM_SMALL_M3) / span;
        if (t > 1.0f)
            t = 1.0f;
    }

    *wet   = LC_WET_SMALL + (LC_WET_LARGE - LC_WET_SMALL) * t;
    *decay = LC_DECAY_SMALL + (LC_DECAY_LARGE - LC_DECAY_SMALL) * t;
}

static void init_channel(lc_reverb_channel* channel, int offset)
{
    memset(channel, 0, sizeof(*channel));
    for (int i = 0; i < LC_COMBS; ++i)
        channel->combs[i].length = kCombDelay[i] + offset;

    for (int i = 0; i < LC_ALLPASSES; ++i)
        channel->allpasses[i].length = kAllpassDelay[i] + offset;
}

static void ensure_ready(void)
{
    if (g_ready)
        return;

    init_channel(&g_channels[0], 0);
    init_channel(&g_channels[1], LC_STEREO_OFFSET);
    g_ready = 1;
}

static float comb_tick(lc_comb* comb, float input, float decay)
{
    const float output = comb->buffer[comb->index];
    comb->store        = output + (comb->store - output) * LC_DAMPING;

    /* A tail decaying below this is inaudible, and letting it run on into denormal floats costs far more
       per sample than the silence is worth in an audio callback */
    if (comb->store > -1.0e-8f && comb->store < 1.0e-8f)
        comb->store = 0.0f;

    comb->buffer[comb->index] = input + comb->store * decay;
    comb->index++;
    if (comb->index >= comb->length)
        comb->index = 0;

    return output;
}

static float allpass_tick(lc_allpass* allpass, float input)
{
    const float delayed = allpass->buffer[allpass->index];
    const float output  = delayed - input;
    allpass->buffer[allpass->index] = input + delayed * LC_ALLPASS_GAIN;
    allpass->index++;
    if (allpass->index >= allpass->length)
        allpass->index = 0;

    return output;
}

static float channel_tick(lc_reverb_channel* channel, float input, float decay)
{
    float value = 0.0f;
    for (int i = 0; i < LC_COMBS; ++i)
        value += comb_tick(&channel->combs[i], input, decay);

    value /= LC_COMBS;
    for (int i = 0; i < LC_ALLPASSES; ++i)
        value = allpass_tick(&channel->allpasses[i], value);

    return value;
}

void lc_reverb_send(const float* samples, int count)
{
    if (!samples || count <= 0)
        return;
    if (count > LC_SEND_CAP)
        count = LC_SEND_CAP;

    for (int i = 0; i < count; ++i)
        g_send[i] += samples[i];

    if (count > g_sendCount)
        g_sendCount = count;
}

static short add_sample(short existing, float value)
{
    const float mixed = (float)existing + value * 32767.0f;
    if (mixed > 32767.0f)
        return 32767;
    if (mixed < -32768.0f)
        return -32768;

    return (short)mixed;
}

void lc_reverb_mix(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    const int sent = g_sendCount;
    g_sendCount    = 0;
    if (!samples || sampleCount <= 0 || channels <= 0) {
        /* The send is accumulated, so leaving this period's voice in it would add to the next one */
        if (sent > 0)
            memset(g_send, 0, sizeof(float) * (size_t)sent);

        return;
    }

    float wet, decay;
    lc_reverb_room_params(bits_to_float(g_roomVolumeBits), &wet, &decay);

    /* Outdoors there is no tail at all. Whatever was ringing is dropped rather than left to leak back in
       on the way through the next doorway. */
    if (wet <= 0.0f) {
        if (sent > 0)
            memset(g_send, 0, sizeof(float) * (size_t)sent);

        g_ready = 0;
        return;
    }

    ensure_ready();

    int left, right;
    lc_audio_find_stereo(channels, channelSpeakerArray, &left, &right);

    /* Channels TeamSpeak has not filled hold whatever was there before: clear them before mixing in */
    if (channelFillMask && channels <= 32) {
        const int used[2] = {left, right};
        for (int u = 0; u < 2; ++u) {
            const unsigned int bit = 1u << used[u];
            if (*channelFillMask & bit)
                continue;

            for (int i = 0; i < sampleCount; ++i)
                samples[i * channels + used[u]] = 0;

            *channelFillMask |= bit;
        }
    }

    for (int i = 0; i < sampleCount; ++i) {
        float input = 0.0f;
        if (i < sent)
            input = g_send[i];

        const float tailLeft  = channel_tick(&g_channels[0], input, decay) * wet;
        const float tailRight = channel_tick(&g_channels[1], input, decay) * wet;
        if (channels == 1) {
            samples[i] = add_sample(samples[i], 0.5f * (tailLeft + tailRight));
        } else {
            samples[i * channels + left]  = add_sample(samples[i * channels + left], tailLeft);
            samples[i * channels + right] = add_sample(samples[i * channels + right], tailRight);
        }
    }

    if (sent > 0)
        memset(g_send, 0, sizeof(float) * (size_t)sent);
}
