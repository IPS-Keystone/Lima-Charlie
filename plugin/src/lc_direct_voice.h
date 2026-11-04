#pragma once

/* Direct speech spatialisation: how loud a speaker is in each ear of the listener. Pure math, no state. */

/* Voice range assumed for a peer that has not announced one. */
#define LC_DEFAULT_VOICE_RANGE 20.0f

/* Positions are world coordinates (Reforger: X east, Y up, Z north); listenerDir is the listener's forward
   vector. muffle is 0 (clear) to 1 (fully muffled). Returns 0 if the speaker is out of range. */
int lc_direct_voice_compute(const float listenerPos[3], const float listenerDir[3], const float speakerPos[3], float voiceRange, float muffle, float* gainLeft, float* gainRight);

/* Vehicle intercom channels, one bit each, as self.ic and players[].ic carry them. */
#define LC_INTERCOM_CREW 1
#define LC_INTERCOM_CARGO 2
#define LC_INTERCOM_MISC 4
#define LC_INTERCOM_ALL 7

/* How loud a speaker's direct speech is on the vehicle intercom, the same in both ears: a fixed level with no
   distance, direction or muffling, since it comes through the listener's headset. 0 unless the two share a
   channel. The game only gives a speaker channels when they are on the listener's own vehicle. */
float lc_intercom_gain(int listenerChannels, int speakerChannels);

/* How long a speaker's muffle and room share take to follow the game: the time constant of an exponential
   glide, so a change is 63% done after this long and 95% after three times it. */
#define LC_MUFFLE_SMOOTH_MS 200.0f

/* current moved toward target as an exponential glide with the given time constant over elapsedMs.
   No time passing leaves it where it is; a time constant of zero or less jumps straight to the target. */
float lc_direct_voice_ease(float current, float target, float elapsedMs, float timeConstantMs);
