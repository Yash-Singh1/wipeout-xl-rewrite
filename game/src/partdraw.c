/* The particle renderer.  On the console this was a hand-written assembly routine (AsmDrawParticles in
   asm/800.s of the decomp); this is its translation to C, running on the GTE of the platform. */
#include "functions.h"
#include "sparks.h"
#include "drawasm.h"
#include "../../platform/psx/gte.h"

extern short CurrentScreen;
extern u_long *OT[2];

/* The streak's colour at its head and tail for each particle type, from the fade level (the particle's life
   scaled to 0..255).  Types without a case keep whatever colour the packet had. */
static void SetStreakColour(LINE_G2 *line, int type, int level)
{
    switch (type) {
    case 1:
        line->r0 = line->g0 = line->b0 = (u_char)level;
        level = (int)((u_int)level >> 1);
        line->r1 = line->g1 = line->b1 = (u_char)level;
        break;
    case 2:
        line->r0 = (u_char)level;
        level = (int)((u_int)level >> 2);
        line->g0 = (u_char)level;
        line->b0 = 0;
        line->r1 = (u_char)level;
        line->g1 = 0;
        line->b1 = 0;
        break;
    case 3:
        line->r0 = line->g0 = line->b0 = 0x30;
        line->r1 = line->g1 = line->b1 = 0x0C;
        break;
    case 4:
        line->r0 = line->g0 = (u_char)((u_int)level >> 2);
        line->b0 = (u_char)level;
        line->r1 = 0;
        line->g1 = 0;
        line->b1 = (u_char)((u_int)level >> 2);
        break;
    case 5:
        line->r0 = line->g0 = line->b0 = 0x60;
        line->r1 = line->g1 = line->b1 = 0x01;
        break;
    }
}

/* Draws the particles of the table as streaks from their last screen position to the current one, linked
   by the depth cue the GTE returns for the projection (IR0, halved).  While `mode` is not above zero the
   particles also move by their velocity and age by one; one that dies or leaves the screen is freed.
   A particle's first drawn frame has no previous position and so draws nothing.  The console's loop
   counted 127 entries of the 128. */
void AsmDrawParticles(SParticle *particles, Skeleton *camera, int mode)
{
    SParticle *particle;
    u_long    *ordering;
    int        remaining;

    SetSkeletonTransform(camera->super);
    ordering = OT[CurrentScreen];
    particle = particles;
    for (remaining = 127; remaining != 0; remaining--, particle++) {
        int       x, y, z;
        int       life;
        int       level;
        LINE_G2  *line;
        int       flag;

        if (particle->type == 0) {
            continue;
        }
        x = particle->pos.vx;
        y = particle->pos.vy;
        z = particle->pos.vz;
        life = particle->life;
        if (mode <= 0) {
            x += particle->vel.vx;
            y += particle->vel.vy;
            z += particle->vel.vz;
            particle->pos.vx = x;
            particle->pos.vy = y;
            particle->pos.vz = z;
            life--;
            particle->life = life;
            if (life <= 0) {
                particle->type = 0;
                particle->flags = 0;
                continue;
            }
        }
        level = life << 5;
        if (level > 255) {
            level = 255;
        }
        line = &particle->line[CurrentScreen != 0];
        SetStreakColour(line, particle->type, level);
        SetLineG2(line);
        SetSemiTrans(line, 1);

        x += camera->relative.t[0];
        y += camera->relative.t[1];
        z += camera->relative.t[2];
        if (!FitsInHalfword(x) || !FitsInHalfword(y) || !FitsInHalfword(z)) {
            particle->type = 0;
            particle->flags = 0;
            continue;
        }
        GteWriteData(0, (x & 0xFFFF) | (int)((u_int)y << 16));
        GteWriteData(1, z);
        GteExecute(GTE_RTPS);
        flag = GteReadControl(31);
        line->x1 = (short)particle->pos.pad;
        line->y1 = (short)(particle->pos.pad >> 16);
        line->x0 = (short)GteReadData(14);
        line->y0 = (short)(GteReadData(14) >> 16);
        particle->pos.pad = GteReadData(14);
        if ((u_int)flag & GTE_REJECT) {
            particle->type = 0;
            particle->flags = 0;
            continue;
        }
        {
            char seen = particle->flags;

            particle->flags = 1;
            if (seen) {
                AddPrim(ordering + ((u_int)GteReadData(8) >> 1), line);
            }
        }
    }
}
