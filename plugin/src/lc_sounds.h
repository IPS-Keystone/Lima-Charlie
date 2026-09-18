#pragma once

/* Radio and UI sounds. Each folder in the sounds directory is a sound set named after the folder; each 16-bit
   PCM WAV in it is a sound named after the file. Radio beep sets hold local_start/local_end (own transmission)
   and remote_start/remote_end (someone else's); the "ui" set holds interface tones. Sounds are mixed into
   TeamSpeak's final playback rather than played through TeamSpeak, so a radio set to one ear beeps in that ear. */

#include "lc_game_state.h"

/* Loads every set under soundsDir (UTF-8), replacing any loaded before. Returns the number of sets. */
int lc_sounds_load(const char* soundsDir);
void lc_sounds_unload(void);

/* Worker thread only. Unknown sets or names (including the "none" set) play nothing. ear is an lc_ear,
   volume 0..1. */
void lc_sounds_play(const char* set, const char* name, int ear, float volume);

/* TeamSpeak audio thread, from ts3plugin_onEditMixedPlaybackVoiceDataEvent. */
void lc_sounds_mix(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);
