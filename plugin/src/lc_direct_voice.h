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
