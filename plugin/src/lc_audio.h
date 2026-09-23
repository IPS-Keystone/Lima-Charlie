#pragma once

/* Per-speaker voice processing in TeamSpeak's playback path. The worker thread publishes a mix (per-client
   direct ear gains and muffle, plus radio ear gains and signal quality); the audio thread applies it without
   locking. */

#include "lc_ts.h"

#define LC_AUDIO_MAX_TARGETS 256
/* Frames of one voice buffer that can go to the room reverb; a TeamSpeak period is far shorter */
#define LC_AUDIO_MAX_SEND 8192

typedef struct {
    anyID client;
    float gainLeft;
    float gainRight;
    float muffle; /* 0 clear .. 1 fully muffled */
    float roomShare; /* 0 .. 1 how much of the listener's room this voice fills, scaling the reverb send */
    float radioLeft;
    float radioRight;
    float radioQuality; /* 1 clean .. near 0 barely readable; garbles the radio path, not its volume */
} lc_voice_target;

/* Worker thread. While active, clients missing from targets are silenced; while inactive, TeamSpeak audio
   is left untouched. */
void lc_audio_publish(int active, const lc_voice_target* targets, int count);

/* Worker thread. Asks the audio thread to forget every talker's filter state before its next buffer. */
void lc_audio_reset(void);

/* TeamSpeak audio thread, from ts3plugin_onEditPostProcessVoiceDataEvent. */
void lc_audio_process(anyID client, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);

/* Channel indices to use as left and right (the same index for mono output). */
void lc_audio_find_stereo(int channels, const unsigned int* channelSpeakerArray, int* left, int* right);
