#include "lc_audio.h"

#include <windows.h>

#include <math.h>
#include <string.h>

#define LC_AUDIO_SAMPLE_RATE 48000.0f
#define LC_AUDIO_MAX_STATES 512
#define LC_FILTER_STAGES 2

/* Low-pass cut-off: effectively open when clear, CLEAR_HZ at the slightest muffle, down to FULL_HZ. */
#define LC_MUFFLE_OPEN_HZ 18000.0f
#define LC_MUFFLE_CLEAR_HZ 8000.0f
#define LC_MUFFLE_FULL_HZ 700.0f

/* Radio voice band: two high-pass then two low-pass stages. */
#define LC_RADIO_HIGHPASS_HZ 300.0f
#define LC_RADIO_LOWPASS_HZ 3400.0f
#define LC_RADIO_FILTERS 4
/* Drive of a clean radio signal, and the speech amplitude the drive stage is level matched at. */
#define LC_DRIVE_CLEAN 1.5f
#define LC_DRIVE_REFERENCE 0.2f
/* Hiss is set from its ratio to the voice rather than as a raw amplitude, closing by even decibel steps
   from one end of the garbled band to the other. A linear amplitude ramp is what the ear notices least
   evenly: it jumps from clean to plainly hissy early on, then barely changes over the rest of the band. */
#define LC_NOISE_SNR_CLEAN_DB 30.0f
#define LC_NOISE_SNR_EDGE_DB (-3.0f)
/* Dropouts last 20 to 80 ms. */
#define LC_DROPOUT_MIN_SAMPLES 960
#define LC_DROPOUT_EXTRA_SAMPLES 2880

typedef struct {
    int              active;
    int              count;
    lc_voice_target targets[LC_AUDIO_MAX_TARGETS];
} lc_audio_snapshot;

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} lc_biquad;

typedef struct {
    anyID      client;
    float      gainLeft;
    float      gainRight;
    float      cutoffHz;
    lc_biquad stages[LC_FILTER_STAGES];

    float        radioLeft;
    float        radioRight;
    lc_biquad   radioFilters[LC_RADIO_FILTERS];
    float        heldSample;
    int          holdCount;
    int          dropoutRemaining;
    float        noise;
    unsigned int rng;
} lc_voice_state;

/* How badly one buffer of radio voice is garbled, derived from signal quality. */
typedef struct {
    float drive;
    float driveNorm;
    int   holdLength;
    float levels;
    float dropoutChance;
    float noiseLevel;
    float voiceLevel;
} lc_garble;

/* Written only by the worker, read by the audio thread. The worker fills the snapshot not being read, then flips. */
static lc_audio_snapshot g_snapshots[2];
static volatile LONG      g_readIndex;

/* Audio thread only. Slot index + 1 per TeamSpeak client id; 0 means no state yet. */
static unsigned short  g_slotOfClient[65536];
static lc_voice_state g_states[LC_AUDIO_MAX_STATES];
static int             g_stateCount;

void lc_audio_publish(int active, const lc_voice_target* targets, int count)
{
    const LONG          writeIndex = g_readIndex ^ 1;
    lc_audio_snapshot* snapshot   = &g_snapshots[writeIndex];
    if (count > LC_AUDIO_MAX_TARGETS)
        count = LC_AUDIO_MAX_TARGETS;
    if (count > 0)
        memcpy(snapshot->targets, targets, sizeof(*targets) * (size_t)count);
    snapshot->count  = count;
    snapshot->active = active;
    InterlockedExchange(&g_readIndex, writeIndex);
}

void lc_audio_find_stereo(int channels, const unsigned int* channelSpeakerArray, int* left, int* right)
{
    *left  = -1;
    *right = -1;
    if (channelSpeakerArray) {
        for (int c = 0; c < channels; ++c) {
            if (*left < 0 && (channelSpeakerArray[c] & (SPEAKER_FRONT_LEFT | SPEAKER_HEADPHONES_LEFT)))
                *left = c;
            if (*right < 0 && (channelSpeakerArray[c] & (SPEAKER_FRONT_RIGHT | SPEAKER_HEADPHONES_RIGHT)))
                *right = c;
        }
    }
    if (*left < 0 || *right < 0) {
        *left  = 0;
        *right = channels > 1 ? 1 : 0;
    }
}

/* Updates coefficients only, keeping the filter's history so cut-off changes do not click. */
static void biquad_set(lc_biquad* q, float cutoffHz, int highPass)
{
    const float w0    = 2.0f * 3.14159265f * cutoffHz / LC_AUDIO_SAMPLE_RATE;
    const float cosw  = (float)cos(w0);
    const float alpha = (float)sin(w0) / (2.0f * 0.70710678f);
    const float a0    = 1.0f + alpha;
    if (highPass) {
        q->b0 = (1.0f + cosw) * 0.5f / a0;
        q->b1 = -(1.0f + cosw) / a0;
    } else {
        q->b0 = (1.0f - cosw) * 0.5f / a0;
        q->b1 = (1.0f - cosw) / a0;
    }
    q->b2 = q->b0;
    q->a1 = -2.0f * cosw / a0;
    q->a2 = (1.0f - alpha) / a0;
}

static float biquad_tick(lc_biquad* q, float x)
{
    const float y = q->b0 * x + q->z1;
    q->z1         = q->b1 * x - q->a1 * y + q->z2;
    q->z2         = q->b2 * x - q->a2 * y;
    return y;
}

static unsigned int next_random(unsigned int* state)
{
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static float random01(unsigned int* state)
{
    return (float)(next_random(state) >> 8) / 16777216.0f;
}

static lc_voice_state* get_state(anyID client)
{
    const unsigned short slot = g_slotOfClient[client];
    if (slot)
        return &g_states[slot - 1];
    if (g_stateCount >= LC_AUDIO_MAX_STATES)
        return NULL;

    lc_voice_state* state = &g_states[g_stateCount];
    memset(state, 0, sizeof(*state));
    state->client   = client;
    state->cutoffHz = LC_MUFFLE_OPEN_HZ;
    for (int i = 0; i < LC_FILTER_STAGES; ++i)
        biquad_set(&state->stages[i], state->cutoffHz, 0);
    for (int i = 0; i < LC_RADIO_FILTERS; ++i)
        biquad_set(&state->radioFilters[i], i < 2 ? LC_RADIO_HIGHPASS_HZ : LC_RADIO_LOWPASS_HZ, i < 2);
    state->rng             = ((unsigned int)client * 2654435761u) | 1u;
    g_slotOfClient[client] = (unsigned short)++g_stateCount;
    return state;
}

/* Every effect below is linear in degrade so the signal worsens steadily across the whole garbled band.
   Curved mappings put nearly all of the audible change in the last fifth of the range, which sounded
   like a clean radio that fell off a cliff. */
static void garble_setup(lc_garble* g, float quality)
{
    const float degrade = quality >= 1.0f ? 0.0f : (quality <= 0.0f ? 1.0f : 1.0f - quality);
    g->drive            = LC_DRIVE_CLEAN + 8.0f * degrade;
    /* Match every drive setting to the clean signal's loudness at a nominal speech amplitude. Normalising
       by the peak instead (1 / tanh(drive)) made a heavily driven signal far louder, because tanh lifts
       everything below full scale: the more static, the louder the voice. */
    const float cleanLevel = (float)(tanh(LC_DRIVE_CLEAN * LC_DRIVE_REFERENCE) / tanh(LC_DRIVE_CLEAN));
    g->driveNorm           = cleanLevel / (float)tanh(g->drive * LC_DRIVE_REFERENCE);
    /* Sample-and-hold down to about 7 kHz. Only long holds are audible on speech, so this is the effect
       that arrives last however it is ramped. */
    g->holdLength = 1 + (int)(degrade * 7.0f);
    /* Speech is transparent above roughly 8 bits, so start near there rather than at 12. */
    g->levels = (float)pow(2.0, 10.0 - 6.0 * degrade);
    /* Dropouts from the point the signal is clearly degraded, rising to a few a second at the edge. */
    g->dropoutChance = degrade > 0.25f ? (degrade - 0.25f) / 0.75f * 0.00010f : 0.0f;
    g->voiceLevel    = 1.0f - 0.3f * degrade;
    const float snrDb = LC_NOISE_SNR_CLEAN_DB + (LC_NOISE_SNR_EDGE_DB - LC_NOISE_SNR_CLEAN_DB) * degrade;
    g->noiseLevel     = LC_DRIVE_REFERENCE * g->voiceLevel * (float)pow(10.0, -snrDb / 20.0);
}

static float radio_tick(lc_voice_state* s, float x, const lc_garble* g)
{
    float v = x;
    for (int i = 0; i < LC_RADIO_FILTERS; ++i)
        v = biquad_tick(&s->radioFilters[i], v);
    v = (float)tanh(v * g->drive) * g->driveNorm;

    if (++s->holdCount >= g->holdLength) {
        s->holdCount  = 0;
        s->heldSample = v;
    }
    v = (float)floor(s->heldSample * g->levels + 0.5f) / g->levels;

    if (s->dropoutRemaining > 0) {
        --s->dropoutRemaining;
        v = 0.0f;
    } else if (g->dropoutChance > 0.0f && random01(&s->rng) < g->dropoutChance) {
        s->dropoutRemaining = LC_DROPOUT_MIN_SAMPLES + (int)(next_random(&s->rng) % LC_DROPOUT_EXTRA_SAMPLES);
    }

    const float white = random01(&s->rng) * 2.0f - 1.0f;
    s->noise += 0.35f * (white - s->noise);
    return v * g->voiceLevel + s->noise * g->noiseLevel;
}

static short to_sample(float value)
{
    const float scaled = value * 32767.0f;
    if (scaled > 32767.0f)
        return 32767;
    if (scaled < -32768.0f)
        return -32768;
    return (short)scaled;
}

void lc_audio_process(anyID client, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    const lc_audio_snapshot* snapshot = &g_snapshots[g_readIndex & 1];
    if (!snapshot->active || !samples || sampleCount <= 0 || channels <= 0)
        return;

    const lc_voice_target* target = NULL;
    for (int i = 0; i < snapshot->count; ++i) {
        if (snapshot->targets[i].client == client) {
            target = &snapshot->targets[i];
            break;
        }
    }

    lc_voice_state* state            = get_state(client);
    const float      targetLeft       = target ? target->gainLeft : 0.0f;
    const float      targetRight      = target ? target->gainRight : 0.0f;
    const float      targetRadioLeft  = target ? target->radioLeft : 0.0f;
    const float      targetRadioRight = target ? target->radioRight : 0.0f;
    const int        useDirect        = targetLeft > 0.0f || targetRight > 0.0f || (state && (state->gainLeft > 0.0f || state->gainRight > 0.0f));
    const int        useRadio         = targetRadioLeft > 0.0f || targetRadioRight > 0.0f || (state && (state->radioLeft > 0.0f || state->radioRight > 0.0f));
    if (!state || (!useDirect && !useRadio)) {
        memset(samples, 0, sizeof(short) * (size_t)sampleCount * (size_t)channels);
        if (state)
            state->gainLeft = state->gainRight = state->radioLeft = state->radioRight = 0.0f;
        return;
    }

    const float muffle = target ? target->muffle : 0.0f;
    const float cutoff = muffle <= 0.01f ? LC_MUFFLE_OPEN_HZ : LC_MUFFLE_CLEAR_HZ * (float)pow(LC_MUFFLE_FULL_HZ / LC_MUFFLE_CLEAR_HZ, muffle > 1.0f ? 1.0f : muffle);
    if (fabs(cutoff - state->cutoffHz) > state->cutoffHz * 0.02f) {
        state->cutoffHz = cutoff;
        for (int i = 0; i < LC_FILTER_STAGES; ++i)
            biquad_set(&state->stages[i], cutoff, 0);
    }

    lc_garble garble;
    garble_setup(&garble, target ? target->radioQuality : 1.0f);

    int left, right;
    lc_audio_find_stereo(channels, channelSpeakerArray, &left, &right);

    /* Ramp gains across the buffer so position and radio changes do not produce zipper noise. */
    const float startLeft       = state->gainLeft;
    const float startRight      = state->gainRight;
    const float startRadioLeft  = state->radioLeft;
    const float startRadioRight = state->radioRight;
    const float step            = 1.0f / (float)sampleCount;
    for (int i = 0; i < sampleCount; ++i) {
        const int base = i * channels;
        float     x    = channels > 1 ? 0.5f * ((float)samples[base + left] + (float)samples[base + right]) : (float)samples[base];
        x /= 32768.0f;

        /* Both direct gains are zero unless useDirect, so the muffle filters would only shape silence. */
        float direct = useDirect ? x : 0.0f;
        if (useDirect) {
            for (int s = 0; s < LC_FILTER_STAGES; ++s)
                direct = biquad_tick(&state->stages[s], direct);
        }
        const float radio = useRadio ? radio_tick(state, x, &garble) : 0.0f;

        const float t        = (float)(i + 1) * step;
        const float outLeft  = direct * (startLeft + (targetLeft - startLeft) * t) + radio * (startRadioLeft + (targetRadioLeft - startRadioLeft) * t);
        const float outRight = direct * (startRight + (targetRight - startRight) * t) + radio * (startRadioRight + (targetRadioRight - startRadioRight) * t);
        for (int c = 0; c < channels; ++c)
            samples[base + c] = 0;
        if (channels == 1) {
            samples[base] = to_sample(0.5f * (outLeft + outRight));
        } else {
            samples[base + left]  = to_sample(outLeft);
            samples[base + right] = to_sample(outRight);
        }
    }

    if (channels > 1 && channels <= 32 && channelFillMask)
        *channelFillMask = (1u << left) | (1u << right);
    state->gainLeft   = targetLeft;
    state->gainRight  = targetRight;
    state->radioLeft  = targetRadioLeft;
    state->radioRight = targetRadioRight;
}
