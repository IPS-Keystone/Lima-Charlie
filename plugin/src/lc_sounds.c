#include "lc_sounds.h"

#include "lc_audio.h"
#include "lc_log.h"
#include "lc_ts.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LC_SOUND_RATE 48000
#define LC_MAX_SOUND_SETS 16
#define LC_MAX_SET_SOUNDS 16
#define LC_MAX_SOUND_SECONDS 5
#define LC_MAX_PLAYING 8
#define LC_PLAY_QUEUE 32
#define LC_BEEP_GAIN 0.7f
/* Volume is quantised to this many steps before a file is rendered, so the cache stays small. Twice the ten
   steps the radio menu offers, which is finer than the product of two ten-step volumes needs. */
#define LC_VOLUME_STEPS 20
/* Rendered variants live here, under the sounds directory; skipped when enumerating sound sets. */
#define LC_CACHE_DIR_NAME L"beepcache"

/* 48 kHz interleaved stereo */
typedef struct {
    char   name[LC_SOUND_NAME_CAP];
    short* frames;
    int    frameCount;
} lc_clip;

typedef struct {
    char     name[LC_BEEP_SET_CAP];
    int      clipCount;
    lc_clip clips[LC_MAX_SET_SOUNDS];
} lc_sound_set;

typedef struct {
    const lc_clip* clip;
    float           gainLeft;
    float           gainRight;
    int             position;
} lc_playing;

/* A beep that stops mid-waveform clicks. Truncating at LC_MAX_SOUND_SECONDS guarantees that, and a
   sound file can end abruptly on its own, so the tail of every clip is faded unless it already ends quiet. */
#define LC_FADE_MS      3
#define LC_FADE_FLOOR   256 /* of 32768: below this the end is inaudible and left alone */

static lc_sound_set g_sets[LC_MAX_SOUND_SETS];
static int           g_setCount;
/* Where rendered ear and volume variants are written; empty when the directory could not be made, which
   leaves every sound on the mixed buffer fallback. Worker thread only. */
static wchar_t       g_cacheDir[MAX_PATH];
static volatile LONG g_ready;
/* Non-zero while the audio thread is inside the mix, so unloading waits rather than freeing clips underneath it */
static volatile LONG g_mixing;

/* Sounds to start: single producer (worker), single consumer (audio thread). */
static lc_playing   g_queue[LC_PLAY_QUEUE];
static volatile LONG g_queueHead;
static volatile LONG g_queueTail;

/* Audio thread only. */
static lc_playing g_playing[LC_MAX_PLAYING];

static unsigned read_u16(const unsigned char* p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned long read_u32(const unsigned char* p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned char* read_whole_file(const wchar_t* path, size_t* size)
{
    FILE* f = _wfopen(path, L"rb");
    if (!f)
        return NULL;
    unsigned char* data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long len = ftell(f);
        if (len > 44 && len < 16 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
            data = (unsigned char*)malloc((size_t)len);
            if (data && fread(data, 1, (size_t)len, f) == (size_t)len) {
                *size = (size_t)len;
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

/* Linear fade over the last few milliseconds, so a clip that was cut mid-waveform does not click. */
static void fade_tail(short* frames, int frameCount)
{
    int fade = LC_SOUND_RATE * LC_FADE_MS / 1000;
    if (fade > frameCount)
        fade = frameCount;
    if (fade < 2)
        return;

    const short* last = frames + (size_t)(frameCount - 1) * 2;
    const int    left = last[0] < 0 ? -last[0] : last[0];
    const int    right = last[1] < 0 ? -last[1] : last[1];
    if (left < LC_FADE_FLOOR && right < LC_FADE_FLOOR)
        return;

    for (int i = 0; i < fade; ++i) {
        const float gain = 1.0f - (float)(i + 1) / (float)fade;
        short*      frame = frames + (size_t)(frameCount - fade + i) * 2;
        frame[0]          = (short)((float)frame[0] * gain);
        frame[1]          = (short)((float)frame[1] * gain);
    }
}

/* 16-bit PCM WAV, mono or stereo, at any sample rate (linearly resampled to 48 kHz). */
static int decode_wav(const unsigned char* data, size_t size, lc_clip* clip)
{
    if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
        return 0;

    unsigned             format = 0, channels = 0, bits = 0;
    unsigned long        rate     = 0;
    const unsigned char* pcm      = NULL;
    size_t               pcmBytes = 0;
    size_t               offset   = 12;
    while (offset + 8 <= size) {
        const unsigned long  chunkSize = read_u32(data + offset + 4);
        const unsigned char* body      = data + offset + 8;
        const size_t         available = size - offset - 8;
        if (memcmp(data + offset, "fmt ", 4) == 0 && chunkSize >= 16 && available >= 16) {
            format   = read_u16(body);
            channels = read_u16(body + 2);
            rate     = read_u32(body + 4);
            bits     = read_u16(body + 14);
        } else if (memcmp(data + offset, "data", 4) == 0) {
            pcm      = body;
            pcmBytes = chunkSize < available ? chunkSize : available;
        }
        offset += 8 + (size_t)chunkSize + (chunkSize & 1);
    }
    if (!pcm || (format != 1 && format != 0xFFFE) || bits != 16 || channels < 1 || channels > 2 || rate < 8000 || rate > 192000)
        return 0;

    long long sourceFrames = (long long)(pcmBytes / (channels * 2));
    if (sourceFrames > (long long)rate * LC_MAX_SOUND_SECONDS)
        sourceFrames = (long long)rate * LC_MAX_SOUND_SECONDS;
    const int frames = (int)(sourceFrames * LC_SOUND_RATE / (long long)rate);
    if (frames < 1)
        return 0;

    clip->frames = (short*)malloc(sizeof(short) * 2 * (size_t)frames);
    if (!clip->frames)
        return 0;
    for (int i = 0; i < frames; ++i) {
        const double    sourcePos = (double)i * (double)rate / LC_SOUND_RATE;
        long long       i0        = (long long)sourcePos;
        if (i0 >= sourceFrames)
            i0 = sourceFrames - 1;
        const long long i1   = i0 + 1 < sourceFrames ? i0 + 1 : i0;
        const double    frac = sourcePos - (double)i0;
        for (unsigned c = 0; c < 2; ++c) {
            const unsigned sourceChannel = channels == 2 ? c : 0;
            const double   a             = (short)read_u16(pcm + (size_t)i0 * channels * 2 + sourceChannel * 2);
            const double   b             = (short)read_u16(pcm + (size_t)i1 * channels * 2 + sourceChannel * 2);
            clip->frames[i * 2 + (int)c] = (short)(a + (b - a) * frac);
        }
    }
    fade_tail(clip->frames, frames);
    clip->frameCount = frames;
    return 1;
}

/* Loads every *.wav in one set folder; returns how many decoded. */
static int load_set(const wchar_t* dir, const wchar_t* setFolder, lc_sound_set* set)
{
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\%s\\*.wav", dir, setFolder);
    pattern[MAX_PATH - 1] = L'\0';

    WIN32_FIND_DATAW found;
    const HANDLE     find = FindFirstFileW(pattern, &found);
    if (find == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || set->clipCount >= LC_MAX_SET_SOUNDS)
            continue;

        lc_clip* clip = &set->clips[set->clipCount];
        memset(clip, 0, sizeof(*clip));
        if (!WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, clip->name, (int)sizeof(clip->name), NULL, NULL))
            continue;
        char* extension = strrchr(clip->name, '.');
        if (extension)
            *extension = '\0';

        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s\\%s\\%s", dir, setFolder, found.cFileName);
        path[MAX_PATH - 1]  = L'\0';
        size_t         size = 0;
        unsigned char* data = read_whole_file(path, &size);
        if (!data)
            continue;
        if (decode_wav(data, size, clip))
            ++set->clipCount;
        else
            lc_logf(LC_LOG_WARNING, "Sound set \"%s\": unsupported file %ls (16-bit PCM WAV expected)", set->name, found.cFileName);
        free(data);
    } while (FindNextFileW(find, &found));
    FindClose(find);
    return set->clipCount;
}

/* Rendered variants from an earlier run, which may have been made from different clips. */
static void purge_cache(void)
{
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\*.wav", g_cacheDir);
    pattern[MAX_PATH - 1] = L'\0';

    WIN32_FIND_DATAW found;
    const HANDLE     find = FindFirstFileW(pattern, &found);
    if (find == INVALID_HANDLE_VALUE)
        return;
    do {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH, L"%s\\%s", g_cacheDir, found.cFileName);
        path[MAX_PATH - 1] = L'\0';
        DeleteFileW(path);
    } while (FindNextFileW(find, &found));
    FindClose(find);
}

int lc_sounds_load(const char* soundsDir)
{
    lc_sounds_unload();

    wchar_t dir[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, soundsDir, -1, dir, MAX_PATH))
        return 0;
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\*", dir);
    pattern[MAX_PATH - 1] = L'\0';

    /* Cleared and remade on every load: the variants are derived from the clips, so a plugin update that
       changes a beep must not leave the old rendering behind. */
    _snwprintf(g_cacheDir, MAX_PATH, L"%s\\%s", dir, LC_CACHE_DIR_NAME);
    g_cacheDir[MAX_PATH - 1] = L'\0';
    if (CreateDirectoryW(g_cacheDir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        purge_cache();
    } else {
        lc_logf(LC_LOG_WARNING, "Could not make %ls; beeps go through the mixed playback buffer instead", g_cacheDir);
        g_cacheDir[0] = L'\0';
    }

    WIN32_FIND_DATAW found;
    const HANDLE     find = FindFirstFileW(pattern, &found);
    if (find == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == L'.')
            continue;
        if (_wcsicmp(found.cFileName, LC_CACHE_DIR_NAME) == 0)
            continue;
        if (g_setCount >= LC_MAX_SOUND_SETS)
            break;

        lc_sound_set* set = &g_sets[g_setCount];
        memset(set, 0, sizeof(*set));
        if (!WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, set->name, (int)sizeof(set->name), NULL, NULL))
            continue;
        if (load_set(dir, found.cFileName, set))
            ++g_setCount;
    } while (FindNextFileW(find, &found));
    FindClose(find);

    InterlockedExchange(&g_ready, g_setCount > 0);
    return g_setCount;
}

void lc_sounds_unload(void)
{
    if (InterlockedExchange(&g_ready, 0)) {
        /* A mix already past the ready check holds pointers into the clips about to be freed, so wait for it
           to leave rather than guessing at a delay. Bounded, because this also runs on shutdown. */
        for (int waited = 0; g_mixing != 0 && waited < 200; ++waited)
            Sleep(1);
    }
    for (int i = 0; i < g_setCount; ++i) {
        for (int s = 0; s < g_sets[i].clipCount; ++s)
            free(g_sets[i].clips[s].frames);
    }
    memset(g_sets, 0, sizeof(g_sets));
    g_setCount = 0;
    memset(g_playing, 0, sizeof(g_playing));
    g_queueHead = 0;
    g_queueTail = 0;
}

/* 48 kHz 16-bit stereo WAV of one clip with the ear and volume already applied. */
static int write_variant(const lc_clip* clip, float gainLeft, float gainRight, const wchar_t* path)
{
    const unsigned long frames = (unsigned long)clip->frameCount;
    const unsigned long bytes  = frames * 4;

    FILE* f = _wfopen(path, L"wb");
    if (!f)
        return 0;

    unsigned char header[44];
    memcpy(header, "RIFF", 4);
    const unsigned long riffSize = 36 + bytes;
    header[4] = (unsigned char)(riffSize & 0xFF);
    header[5] = (unsigned char)((riffSize >> 8) & 0xFF);
    header[6] = (unsigned char)((riffSize >> 16) & 0xFF);
    header[7] = (unsigned char)((riffSize >> 24) & 0xFF);
    memcpy(header + 8, "WAVEfmt ", 8);
    static const unsigned char fmt[20] = {16, 0, 0, 0,      /* chunk size */
                                          1,  0,            /* PCM */
                                          2,  0,            /* stereo */
                                          0x80, 0xBB, 0, 0, /* 48000 a second */
                                          0x00, 0xEE, 0x02, 0, /* 192000 bytes a second */
                                          4,  0,            /* block align */
                                          16, 0};           /* bits a sample */
    memcpy(header + 16, fmt, sizeof(fmt));
    memcpy(header + 36, "data", 4);
    header[40] = (unsigned char)(bytes & 0xFF);
    header[41] = (unsigned char)((bytes >> 8) & 0xFF);
    header[42] = (unsigned char)((bytes >> 16) & 0xFF);
    header[43] = (unsigned char)((bytes >> 24) & 0xFF);
    if (fwrite(header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return 0;
    }

    int ok = 1;
    for (int i = 0; i < clip->frameCount && ok; ++i) {
        const float l = (float)clip->frames[i * 2] * gainLeft;
        const float r = (float)clip->frames[i * 2 + 1] * gainRight;
        short       frame[2];
        frame[0] = l > 32767.0f ? 32767 : (l < -32768.0f ? -32768 : (short)l);
        frame[1] = r > 32767.0f ? 32767 : (r < -32768.0f ? -32768 : (short)r);
        ok       = fwrite(frame, sizeof(frame), 1, f) == 1;
    }
    fclose(f);
    if (!ok)
        DeleteFileW(path);

    return ok;
}

/* Hands the clip to TeamSpeak's own player, rendering the variant for this ear and volume if it is the first
   time that pairing has been asked for. Returns 0 if anything stops it, so the caller can fall back. */
static int play_through_teamspeak(const lc_clip* clip, const char* set, int ear, float volume)
{
    if (!g_cacheDir[0] || !g_ts3.playWaveFile || !g_ts3.getCurrentServerConnectionHandlerID)
        return 0;

    const uint64 sch = g_ts3.getCurrentServerConnectionHandlerID();
    if (!sch)
        return 0;

    int step = (int)(volume * LC_VOLUME_STEPS + 0.5f);
    if (step < 1)
        return 0;
    if (step > LC_VOLUME_STEPS)
        step = LC_VOLUME_STEPS;

    wchar_t path[MAX_PATH];
    if (_snwprintf(path, MAX_PATH, L"%s\\%hs_%hs_e%d_v%02d.wav", g_cacheDir, set, clip->name, ear, step) < 0)
        return 0;

    path[MAX_PATH - 1] = L'\0';
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        const float gain = LC_BEEP_GAIN * (float)step / (float)LC_VOLUME_STEPS;
        if (!write_variant(clip, ear == LC_EAR_RIGHT ? 0.0f : gain, ear == LC_EAR_LEFT ? 0.0f : gain, path)) {
            lc_logf(LC_LOG_WARNING, "Could not write %ls; beeps fall back to the mixed buffer", path);
            g_cacheDir[0] = L'\0';
            return 0;
        }
    }

    char utf8[MAX_PATH * 3];
    if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, (int)sizeof(utf8), NULL, NULL))
        return 0;

    return g_ts3.playWaveFile(sch, utf8) == ERROR_ok;
}

void lc_sounds_play(const char* set, const char* name, int ear, float volume)
{
    if (!g_ready || !set || !set[0] || !name || !name[0] || volume <= 0.0f)
        return;

    const lc_clip* clip = NULL;
    for (int i = 0; i < g_setCount && !clip; ++i) {
        if (strcmp(g_sets[i].name, set) != 0)
            continue;
        for (int s = 0; s < g_sets[i].clipCount; ++s) {
            if (strcmp(g_sets[i].clips[s].name, name) == 0) {
                clip = &g_sets[i].clips[s];
                break;
            }
        }
        break;
    }
    if (!clip)
        return;

    if (volume > 1.0f)
        volume = 1.0f;

    /* TeamSpeak's own player owns its mixing, so nothing another plugin does to the shared buffer can reach
       this. Only when it cannot be used does the sound go into that buffer instead. */
    if (play_through_teamspeak(clip, set, ear, volume))
        return;

    const float gain = LC_BEEP_GAIN * volume;
    const LONG  head = g_queueHead;
    const LONG  next = (head + 1) % LC_PLAY_QUEUE;
    if (next == g_queueTail)
        return;
    g_queue[head].clip      = clip;
    g_queue[head].gainLeft  = ear == LC_EAR_RIGHT ? 0.0f : gain;
    g_queue[head].gainRight = ear == LC_EAR_LEFT ? 0.0f : gain;
    g_queue[head].position  = 0;
    InterlockedExchange(&g_queueHead, next);
}

static void add_sample(short* sample, float value)
{
    const float mixed = (float)*sample + value;
    *sample           = mixed > 32767.0f ? 32767 : (mixed < -32768.0f ? -32768 : (short)mixed);
}

static void mix_playing(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    while (g_queueTail != g_queueHead) {
        const LONG tail = g_queueTail;
        int        slot = -1;
        for (int i = 0; i < LC_MAX_PLAYING; ++i) {
            if (!g_playing[i].clip) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            /* Eight beeps at once already: cutting the one nearest its end is the least audible way to make
               room, where taking the first slot every time could chop a beep off at full amplitude. */
            int nearest   = 0;
            int fewestLeft = g_playing[0].clip->frameCount - g_playing[0].position;
            for (int i = 1; i < LC_MAX_PLAYING; ++i) {
                const int left = g_playing[i].clip->frameCount - g_playing[i].position;
                if (left < fewestLeft) {
                    fewestLeft = left;
                    nearest    = i;
                }
            }
            slot = nearest;
        }
        g_playing[slot] = g_queue[tail];
        InterlockedExchange(&g_queueTail, (tail + 1) % LC_PLAY_QUEUE);
    }

    int any = 0;
    for (int i = 0; i < LC_MAX_PLAYING && !any; ++i)
        any = g_playing[i].clip != NULL;
    if (!any)
        return;

    int left, right;
    lc_audio_find_stereo(channels, channelSpeakerArray, &left, &right);

    /* Beeps are added to the finished mix, never cleared into it: this buffer already holds every voice,
       and zeroing a channel TeamSpeak had not marked as filled cut the speech underneath the beep. */
    if (channelFillMask && channels <= 32)
        *channelFillMask |= (1u << left) | (1u << right);

    for (int p = 0; p < LC_MAX_PLAYING; ++p) {
        lc_playing* playing = &g_playing[p];
        if (!playing->clip)
            continue;
        const int remaining = playing->clip->frameCount - playing->position;
        const int count     = remaining < sampleCount ? remaining : sampleCount;
        for (int i = 0; i < count; ++i) {
            const short* frame = playing->clip->frames + (playing->position + i) * 2;
            const float  l     = (float)frame[0] * playing->gainLeft;
            const float  r     = (float)frame[1] * playing->gainRight;
            if (channels == 1) {
                add_sample(&samples[i], 0.5f * (l + r));
            } else {
                add_sample(&samples[i * channels + left], l);
                add_sample(&samples[i * channels + right], r);
            }
        }
        playing->position += count;
        if (playing->position >= playing->clip->frameCount)
            playing->clip = NULL;
    }
}

void lc_sounds_mix(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
    if (!samples || sampleCount <= 0 || channels <= 0)
        return;

    /* Claimed before the ready check is read, so an unload that clears it cannot free clips while this runs. */
    InterlockedIncrement(&g_mixing);
    if (g_ready)
        mix_playing(samples, sampleCount, channels, channelSpeakerArray, channelFillMask);

    InterlockedDecrement(&g_mixing);
}
