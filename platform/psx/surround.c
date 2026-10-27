/* Headphone surround. A Dolby Surround receiver plays what a game encodes in opposite phase on the two sides from
   speakers behind the listener; on headphones the encoding only sounds hollow. Wipeout XL's Stereo mode encodes every
   sound effect so: one in the middle plays from behind alone, one panned to a side from that side's front speaker
   too. Here the SPU decodes it per voice (spu.h's rear) and each ear hears what the speakers would give it:

   - the front pair at about 30 degrees: each ear also hears the other side, a little later and duller (crossfeed),
     as the head lets through from a speaker on the far side;
   - the rear pair at about 120 degrees: the far ear hears it later (0.66 ms) and much duller (the head's shadow),
     both ears without the treble the outer ear takes from a sound behind, and a quiet reflection from behind
     (a few metres away, a little differently for each ear) puts it outside the head.

   This is a simple model, not measured head responses; it is meant to be heard as "behind", not to match a room. */
#include "surround.h"

#include <math.h>
#include <string.h>

#include "spu.h"

#define DELAY_SIZE 1024 /* (a power of two, above the longest delay) */

typedef struct Ear {
    float frontCross;  /* the other front speaker's sound, low-passed */
    float rearNear;    /* this side's rear speaker, low-passed */
    float rearFar;     /* the other side's rear speaker, low-passed */
    float reflection;  /* the reflection from behind, low-passed */
} Ear;

static struct {
    float front[2][DELAY_SIZE];
    float rear[2][DELAY_SIZE];
    float rearMono[DELAY_SIZE];
    unsigned at;
    Ear ear[2];
} state;

/* A one-pole low-pass filter's coefficient for a cut-off in Hz. */
static float Pole(float hz)
{
    return 1.0f - expf(-2.0f * 3.14159265f * hz / (float)SPU_SAMPLE_RATE);
}

static float Delayed(const float *line, unsigned samples)
{
    return line[(state.at - samples) & (DELAY_SIZE - 1)];
}

void SurroundReset(void)
{
    memset(&state, 0, sizeof(state));
}

void SurroundRender(const int16_t front[2], const int16_t rear[2], int16_t out[2])
{
    static float crossPole, nearPole, farPole, reflectionPole;
    /* the delays in samples at 44.1 kHz: the front's from one ear to the other 0.3 ms, the rear's 0.66 ms; the
       reflection 11 and 12 ms */
    static const unsigned frontDelay = 13, rearDelay = 29, reflectionDelay[2] = {485, 530};
    float mixed[2];
    unsigned c;

    if (!crossPole) {
        crossPole = Pole(700.0f);
        nearPole = Pole(5000.0f);
        farPole = Pole(1000.0f);
        reflectionPole = Pole(3000.0f);
    }
    for (c = 0; c < 2; c++) {
        state.front[c][state.at] = front[c];
        state.rear[c][state.at] = rear[c];
    }
    state.rearMono[state.at] = 0.5f * ((float)rear[0] + (float)rear[1]);
    for (c = 0; c < 2; c++) {
        Ear *ear = &state.ear[c];
        unsigned other = 1 - c;

        ear->frontCross += crossPole * (Delayed(state.front[other], frontDelay) - ear->frontCross);
        ear->rearNear += nearPole * ((float)rear[c] - ear->rearNear);
        ear->rearFar += farPole * (Delayed(state.rear[other], rearDelay) - ear->rearFar);
        ear->reflection += reflectionPole * (Delayed(state.rearMono, reflectionDelay[c]) - ear->reflection);
        mixed[c] = 0.85f * ((float)front[c] + 0.3f * ear->frontCross) +
                   0.9f * ear->rearNear + 0.45f * ear->rearFar + 0.2f * ear->reflection;
    }
    state.at = (state.at + 1) & (DELAY_SIZE - 1);
    for (c = 0; c < 2; c++) {
        float v = mixed[c] > 32767.0f ? 32767.0f : mixed[c] < -32768.0f ? -32768.0f : mixed[c];

        out[c] = (int16_t)lrintf(v);
    }
}
