#pragma once

/* Direct speech spatialisation: how loud a speaker is in each ear of the listener. Pure math, no state. */

/* Voice range assumed for a peer that has not announced one. */
#define LC_DEFAULT_VOICE_RANGE 20.0f

/* Positions are world coordinates (Reforger: X east, Y up, Z north); listenerDir is the listener's forward
   vector. muffle is 0 (clear) to 1 (fully muffled). Returns 0 if the speaker is out of range. */
int lc_direct_voice_compute(const float listenerPos[3], const float listenerDir[3], const float speakerPos[3], float voiceRange, float muffle, float* gainLeft, float* gainRight);
