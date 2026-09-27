#pragma once

/* Radio and UI sounds. Each folder in the sounds directory is a sound set named after the folder; each 16-bit
   PCM WAV in it is a sound named after the file. Radio beep sets hold local_start/local_end (own transmission)
   and remote_start/remote_end (someone else's); the "ui" set holds interface tones.

   Sounds are handed to TeamSpeak's own player, which mixes them itself. The ear and the volume are baked into
   a rendered copy of the clip under "beepcache", because that API takes a path and nothing else: one file per
   set, sound, ear and twentieth of volume, written the first time that combination is asked for.

   The alternative is writing them into the mixed playback buffer, which is what this did until 1.0.13. That
   buffer is shared with every other plugin the client has loaded, in load order, and a plugin that overwrites
   rather than adds to it destroys whatever we put there. Coalition VON did exactly that on a tester's machine.
   The buffer path is kept as a fallback for when the cache cannot be written. */

#include "lc_game_state.h"

/* Loads every set under soundsDir (UTF-8), replacing any loaded before. Returns the number of sets. */
int lc_sounds_load(const char* soundsDir);
void lc_sounds_unload(void);

/* Worker thread only. Unknown sets or names (including the "none" set) play nothing. ear is an lc_ear,
   volume 0..1. */
void lc_sounds_play(const char* set, const char* name, int ear, float volume);

/* TeamSpeak audio thread, from ts3plugin_onEditMixedPlaybackVoiceDataEvent. */
void lc_sounds_mix(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);
