#pragma once

/* Room reverb for direct speech.
   Per-speaker processing cannot reverberate on its own: a room's tail belongs to the room, not to one
   voice, so every direct voice adds into one shared send while it is processed, and the tail is rendered
   once over the mixed playback buffer. The send is accumulated and consumed on TeamSpeak's audio thread,
   in the order TeamSpeak calls it: every per-client edit, then the mixed edit.

   The room's size decides how much tail there is and how long it lasts; outdoors there is none, and radio
   voices never contribute. */

/* Audio thread, from lc_audio_process: adds one buffer of dry direct voice to the send.
   \param samples mono frames, already shaped by distance, muffle and gain
   \param count number of frames */
void lc_reverb_send(const float* samples, int count);

/* Audio thread, from ts3plugin_onEditMixedPlaybackVoiceDataEvent: renders the tail of whatever was sent
   for this period and mixes it into the playback buffer. */
void lc_reverb_mix(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);

/* Worker thread: the volume in cubic metres of the room the listener is standing in, 0 outdoors. */
void lc_reverb_set_room(float volumeM3);

/* How wet and how long the tail is for a room of this volume, exposed for the offline tests.
   \param wet 0 when there is no room at all
   \param decay fraction of the tail remaining after one comb pass */
void lc_reverb_room_params(float volumeM3, float* wet, float* decay);
