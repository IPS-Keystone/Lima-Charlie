#include "lc_direct_voice.h"

#include <math.h>

#define LC_PI 3.14159265f
#define LC_SQRT2 1.41421356f

/* Attenuation reached at the edge of the voice range, before the final fade. */
#define LC_RANGE_EDGE_DB -30.0f
/* Fraction of the range after which the voice fades linearly to silence, so the cut-off is not audible. */
#define LC_EDGE_FADE_START 0.85f
/* Below 1, a speaker fully to one side is still faintly audible in the other ear. */
#define LC_PAN_WIDTH 0.85f
/* Extra attenuation for a speaker directly behind the listener. */
#define LC_BEHIND_ATTENUATION 0.25f
/* Attenuation at full muffle; the plugin's low-pass filter does the rest. */
#define LC_MUFFLE_ATTENUATION_DB -9.0f

static float clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

static float db_to_gain(float db)
{
    return (float)pow(10.0, db / 20.0);
}

int lc_direct_voice_compute(const float listenerPos[3], const float listenerDir[3], const float speakerPos[3], float voiceRange, float muffle, float* gainLeft, float* gainRight)
{
    *gainLeft  = 0.0f;
    *gainRight = 0.0f;
    if (voiceRange <= 0.0f)
        voiceRange = LC_DEFAULT_VOICE_RANGE;

    const float dx       = speakerPos[0] - listenerPos[0];
    const float dy       = speakerPos[1] - listenerPos[1];
    const float dz       = speakerPos[2] - listenerPos[2];
    const float distance = (float)sqrt(dx * dx + dy * dy + dz * dz);
    if (distance >= voiceRange)
        return 0;

    const float x    = distance / voiceRange;
    float       gain = db_to_gain(LC_RANGE_EDGE_DB * (float)pow(x, 1.5));
    if (x > LC_EDGE_FADE_START)
        gain *= (1.0f - x) / (1.0f - LC_EDGE_FADE_START);

    /* Horizontal plane only: forward from the listener's view, right = up x forward = (fz, -fx). */
    float       forwardX = listenerDir[0];
    float       forwardZ = listenerDir[2];
    const float forwardLen = (float)sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (forwardLen < 1e-4f) {
        forwardX = 0.0f;
        forwardZ = 1.0f;
    } else {
        forwardX /= forwardLen;
        forwardZ /= forwardLen;
    }

    float       pan        = 0.0f;
    float       front      = 1.0f;
    const float horizontal = (float)sqrt(dx * dx + dz * dz);
    if (horizontal > 0.25f) {
        const float toX = dx / horizontal;
        const float toZ = dz / horizontal;
        pan             = toX * forwardZ - toZ * forwardX;
        front           = toX * forwardX + toZ * forwardZ;
    }
    pan *= LC_PAN_WIDTH;

    if (front < 0.0f)
        gain *= 1.0f - LC_BEHIND_ATTENUATION * -front;
    gain *= db_to_gain(LC_MUFFLE_ATTENUATION_DB * clamp01(muffle));

    /* Equal-power pan, scaled so a centred speaker gets full gain in both ears. */
    const float angle = (pan + 1.0f) * LC_PI * 0.25f;
    const float left  = gain * (float)cos(angle) * LC_SQRT2;
    const float right = gain * (float)sin(angle) * LC_SQRT2;
    *gainLeft         = left > 1.0f ? 1.0f : left;
    *gainRight        = right > 1.0f ? 1.0f : right;
    return 1;
}
