#include "functions.h"
#include "sparks.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
int           snowCount;
u_char        birdDelay;
TrackSection *birdSectionA;
TrackSection *birdSectionB;
POLY_FT4     *birdPrims;
Bird         *birds;
int           missflareat;
int           rockflareat;
int           rockang;
int           pitsparkat;
int           exdelay;
u8            partpos;
extern HeapBlock    *heap;
extern void         *reserveHeap;
extern TrackCamera  *cameraPtr;
extern ShipData     *shipIndex;
extern int           ownShip;
extern short         CurrentScreen;
extern int         *OT[];
extern short         TextureTableCount;
extern Texture      *textureTable[];
extern WADFILE      *textureWad;
extern Texture      *explodeTexture[];
extern Texture      *birdTexture[];
extern char          birdTextureFile[];
extern Ring          rings[];
extern POLY_G4       ringPrims[2][16][RING_COUNT];
extern POLY_G3       missileFlarePrims[2][4][8];
extern POLY_G3       rocketFlarePrims[2][4][15];
extern Explosion     explosions[];
extern POLY_FT4      explodePrims[EXPLOSION_COUNT][2];
extern Electric      electrics[];

SParticle *InitParticle(void)
{
    SParticle *particles;

    particles = DAlloc(heap, PARTICLE_COUNT * sizeof(SParticle));
    ClearParticleTable(particles);
    return particles;
}


void AddRandomParticles(SParticle *particles, int count, VECTOR *pos, VECTOR *vel)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx;
            particles[i].pos.vy = pos->vy;
            particles[i].pos.vz = pos->vz;
            particles[i].vel.vx = (newRand() & 0x3F) - 32 + vel->vx;
            particles[i].vel.vy = (newRand() & 0x3F) - 32;
            particles[i].vel.vz = (newRand() & 0x3F) - 32 + vel->vz;
            particles[i].life = 7;
            particles[i].type = 1;
            particles[i].flags = 0;
        }
    }
}


void AddScrapingParticles(SParticle *particles, int count, VECTOR *pos, VECTOR *vel)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx;
            particles[i].pos.vy = pos->vy;
            particles[i].pos.vz = pos->vz;
            particles[i].vel.vx = (newRand() & 0x3F) - 32 + vel->vx;
            particles[i].vel.vy = (newRand() & 0x3F) - 32;
            particles[i].vel.vz = (newRand() & 0x3F) - 32 + vel->vz;
            particles[i].life = 7;
            particles[i].type = 2;
            particles[i].flags = 0;
        }
    }
}


void AddSnow(SParticle *particles, int unused, VECTOR *camera)
{
    int           i;
    int           n;
    TrackSection *section;

    birdDelay = (birdDelay + 1) & 3;
    if (!(birdDelay & 1)) {
        section = cameraPtr->section;
        for (n = 0; n < 8; n++) {
            if (section->snow == 0) {
                i = snowCount;
                snowCount = (i + 1) % PARTICLE_COUNT;
                if (i >= 0) {
                    if (particles[i].type == 0) {
                        particles[i].pos.vx = section->centre.vx + (newRand() & 0x1FFF) - 0x1000;
                        particles[i].pos.vy = section->centre.vy - ((newRand() + 0xC18) & 0xFFF);
                        particles[i].pos.vz = section->centre.vz + (newRand() & 0x1FFF) - 0x1000;
                        particles[i].vel.vx = 4;
                        particles[i].vel.vy = 96;
                        particles[i].life = 50;
                        particles[i].vel.vz = 0;
                        particles[i].type = 5;
                        particles[i].flags = 0;
                    }
                }
            }
            section = section->nextSection.ptr;
        }
        section = cameraPtr->section->prevSection.ptr;
        for (n = 0; n < 7; n++) {
            if (section->snow == 0) {
                i = snowCount;
                snowCount = (i + 1) % PARTICLE_COUNT;
                if (i >= 0) {
                    if (particles[i].type == 0) {
                        particles[i].pos.vx = section->centre.vx + (newRand() & 0x1FFF) - 0x1000;
                        particles[i].pos.vy = section->centre.vy - ((newRand() + 0xFFF) & 0xFFF);
                        particles[i].pos.vz = section->centre.vz + (newRand() & 0x1FFF) - 0x1000;
                        particles[i].vel.vx = 4;
                        particles[i].vel.vy = 96;
                        particles[i].life = 50;
                        particles[i].vel.vz = 0;
                        particles[i].type = 5;
                        particles[i].flags = 0;
                    }
                }
            }
            section = section->prevSection.ptr;
        }
    }
}


void AddRain(SParticle *particles, int unused, VECTOR *camera)
{
    int           i;
    int           n;
    TrackSection *section;

    section = cameraPtr->section;
    for (n = 0; n < 8; n += 2) {
        if (!section->snow) {
            i = snowCount;
            snowCount = (i + 1) % PARTICLE_COUNT;
            if (i >= 0) {
                particles[i].pos.vx = section->centre.vx + (newRand() & 0x1FFF) - 0x1000;
                particles[i].pos.vy = -camera->vy - ((newRand() + 0x1000) & 0xFFF);
                particles[i].pos.vz = section->centre.vz + (newRand() & 0x1FFF) - 0x1000;
                particles[i].vel.vx = 160;
                particles[i].vel.vy = 480;
                particles[i].life = 20;
                particles[i].vel.vz = 0;
                particles[i].type = 3;
                particles[i].flags = 0;
            }
        }
        section = section->nextSection.ptr;
        section = section->nextSection.ptr;
    }
    section = cameraPtr->section->prevSection.ptr;
    for (n = 0; n < 7; n += 2) {
        if (!section->snow) {
            i = snowCount;
            snowCount = (i + 1) % PARTICLE_COUNT;
            if (i >= 0) {
                particles[i].pos.vx = section->centre.vx + (newRand() & 0x1FFF) - 0x1000;
                particles[i].pos.vy = -camera->vy - ((newRand() + 0x1000) & 0xFFF);
                particles[i].pos.vz = section->centre.vz + (newRand() & 0x1FFF) - 0x1000;
                particles[i].vel.vx = 160;
                particles[i].vel.vy = 480;
                particles[i].life = 20;
                particles[i].vel.vz = 0;
                particles[i].type = 3;
                particles[i].flags = 0;
            }
        }
        section = section->prevSection.ptr;
        section = section->prevSection.ptr;
    }
}


void AddExplodingParticles(SParticle *particles, int count, VECTOR *pos)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx + ((newRand() & 0xFF) - 0x7F);
            particles[i].pos.vy = pos->vy + ((newRand() & 0xFF) - 0x7F);
            particles[i].pos.vz = pos->vz + ((newRand() & 0xFF) - 0x7F);
            particles[i].vel.vx = (newRand() & 0x7F) - 0x3F;
            particles[i].vel.vy = (newRand() & 0x7F) - 0x3F;
            particles[i].vel.vz = (newRand() & 0x7F) - 0x3F;
            particles[i].life = 15;
            particles[i].type = 1;
            particles[i].flags = 0;
        }
    }
}


void AddEboltParticles(SParticle *particles, int count, VECTOR *pos)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx;
            particles[i].pos.vy = pos->vy;
            particles[i].pos.vz = pos->vz;
            particles[i].vel.vx = (newRand() & 0x3F) - 0x1F;
            particles[i].vel.vy = (newRand() & 0x3F) - 0x1F;
            particles[i].vel.vz = (newRand() & 0x3F) - 0x1F;
            particles[i].life = 15;
            particles[i].type = 4;
            particles[i].flags = 0;
        }
    }
}


void AddRisingParticles(SParticle *particles, int count, VECTOR *pos, int fallSpeed)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx + (newRand() % 1600) - 800;
            particles[i].pos.vy = pos->vy;
            particles[i].pos.vz = pos->vz + (newRand() % 1600) - 800;
            particles[i].vel.vx = 0;
            particles[i].vel.vy = -(newRand() & 0xF) * fallSpeed;
            particles[i].vel.vz = 0;
            particles[i].life = (newRand() & 7) + 8;
            particles[i].type = 2;
            particles[i].flags = 0;
        }
    }
}


void AddFireyParticles(SParticle *particles, int count, VECTOR *pos, int dir, int type, VECTOR *vel)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx;
            particles[i].pos.vy = pos->vy;
            particles[i].pos.vz = pos->vz;
            particles[i].vel.vx = ((newRand() & 7) - 4) + vel->vx;
            particles[i].vel.vy = -((newRand() & 7) + 8) * dir + vel->vy;
            particles[i].vel.vz = ((newRand() & 7) - 4) + vel->vz;
            particles[i].life = (newRand() & 7) + 20;
            particles[i].type = type;
            particles[i].flags = 0;
        }
    }
}


void AddPitParticles(SParticle *particles, int count, VECTOR *pos, int fallSpeed)
{
    int i;

    for (; count > 0; count--) {
        i = snowCount;
        snowCount = (i + 1) % PARTICLE_COUNT;
        if (i >= 0) {
            particles[i].pos.vx = pos->vx + (newRand() & 0x1FF) - 0x100;
            particles[i].pos.vy = pos->vy + 0x100;
            particles[i].pos.vz = pos->vz + (newRand() & 0x1FF) - 0x100;
            particles[i].vel.vx = 0;
            particles[i].vel.vy = -(newRand() & 7) * fallSpeed;
            particles[i].vel.vz = 0;
            particles[i].life = (newRand() & 7) + 8;
            particles[i].type = 2;
            particles[i].flags = 0;
        }
    }
}


void ClearParticleTable(SParticle *particles)
{
    int i;

    for (i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].pos.vx = 0;
        particles[i].pos.vy = 0;
        particles[i].pos.vz = 0;
        particles[i].vel.vx = 0;
        particles[i].vel.vy = 0;
        particles[i].vel.vz = 0;
        particles[i].life = 0;
        particles[i].type = 0;
        particles[i].flags = 0;
    }
}


void LoadExplodeGFX(void)
{
    int i;
    int start;

    start = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(textureWad));
    for (i = 0; i < EXPLOSION_COUNT; i++) {
        explodeTexture[i] = textureTable[start++];
    }
}


void InitBirds(void)
{
    int i;
    int j;

    birdSectionA = shipIndex[ownShip].nearTrkSect;
    birdSectionB = birdSectionA;
    for (j = 0; j < 50; j++) {
        birdSectionB = birdSectionB->nextSection.ptr;
    }
    birdPrims = DAlloc(reserveHeap, BIRD_COUNT * 2 * sizeof(POLY_FT4));
    birds = DAlloc(reserveHeap, BIRD_COUNT * sizeof(Bird));
    for (i = 0; i < BIRD_COUNT; i++) {
        birds[i].x = (newRand() & 0xFFFF) - 0x8000;
        birds[i].y = -2048;
        birds[i].z = (newRand() & 0xFFFF) - 0x8000;
        birds[i].vx = 0;
        birds[i].vy = 0;
        birds[i].vz = 0;
        birds[i].accel = (newRand() & 7) + 4;
        birds[i].frame = newRand() & 31;
        for (j = 0; j < 2; j++) {
            SetPolyFT4(&birdPrims[i * 2 + j]);
            birdPrims[i * 2 + j].r0 = 128;
            birdPrims[i * 2 + j].g0 = 128;
            birdPrims[i * 2 + j].b0 = 128;
            birdPrims[i * 2 + j].x0 = 128;
            birdPrims[i * 2 + j].y0 = 64;
            birdPrims[i * 2 + j].x1 = 192;
            birdPrims[i * 2 + j].y1 = 64;
            birdPrims[i * 2 + j].x2 = 128;
            birdPrims[i * 2 + j].y2 = 128;
            birdPrims[i * 2 + j].x3 = 192;
            birdPrims[i * 2 + j].y3 = 128;
            birdPrims[i * 2 + j].u0 = birdTexture[0]->u0;
            birdPrims[i * 2 + j].v0 = birdTexture[0]->v0;
            birdPrims[i * 2 + j].u1 = birdTexture[0]->u1;
            birdPrims[i * 2 + j].v1 = birdTexture[0]->v1;
            birdPrims[i * 2 + j].u2 = birdTexture[0]->u2;
            birdPrims[i * 2 + j].v2 = birdTexture[0]->v2;
            birdPrims[i * 2 + j].u3 = birdTexture[0]->u3;
            birdPrims[i * 2 + j].v3 = birdTexture[0]->v3;
            birdPrims[i * 2 + j].tpage = GetTPage(0, 0, birdTexture[0]->textureX, birdTexture[0]->textureY);
            birdPrims[i * 2 + j].clut = birdTexture[0]->cba;
        }
    }
}


void LoadBirdGFX(void)
{
    int i;
    int start;

    start = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence(birdTextureFile));
    for (i = 0; i < BIRD_COUNT; i++) {
        birdTexture[i] = textureTable[start++];
    }
    InitBirds();
}


void InitExplode(void)
{
    int i, j;
    POLY_FT4 *quad;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        explosions[i].x = 0;
        explosions[i].y = 0;
        explosions[i].z = 0;
        explosions[i].r = 128;
        explosions[i].g = 128;
        explosions[i].b = 128;
        explosions[i].flash = 0;
        explosions[i].ship = -1;
        explosions[i].size = 0;
        explosions[i].life = 0;
        for (j = 0; j < 2; j++) {
            SetPolyFT4(&explodePrims[i][j]);
            quad = (POLY_FT4 *)&((u_char *)explodePrims)[i * 2 * sizeof(POLY_FT4) + j * sizeof(POLY_FT4)];
            explodePrims[i][j].r0 = 128;
            explodePrims[i][j].g0 = 128;
            explodePrims[i][j].b0 = 128;
            explodePrims[i][j].x0 = 128;
            explodePrims[i][j].y0 = 64;
            explodePrims[i][j].x1 = 192;
            explodePrims[i][j].y1 = 64;
            explodePrims[i][j].x2 = 128;
            explodePrims[i][j].y2 = 128;
            explodePrims[i][j].x3 = 192;
            explodePrims[i][j].y3 = 128;
            quad->u0 = explodeTexture[0]->u0;
            quad->v0 = explodeTexture[0]->v0;
            quad->u1 = explodeTexture[0]->u1;
            quad->v1 = explodeTexture[0]->v1;
            quad->u2 = explodeTexture[0]->u2;
            quad->v2 = explodeTexture[0]->v2;
            quad->u3 = explodeTexture[0]->u3;
            quad->v3 = explodeTexture[0]->v3;
            quad->tpage = GetTPage(1, 1, explodeTexture[0]->textureX, explodeTexture[0]->textureY);
            SetSemiTrans(&explodePrims[i][j], 1);
            explodePrims[i][j].clut = explodeTexture[0]->cba;
        }
    }
}


#define EXPLODE_QUAD(i) (&explodePrims[i][CurrentScreen])

void DrawExplode(void)
{
    SVECTOR vertex;
    VECTOR relPos;
    DVECTOR sxy;
    VECTOR camera;
    int p, flag;
    int i, frame, depth;
    int x, z, absX, absZ;
    int size, left, top;
    short radius;
    int maxSize;

    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    partpos++;
    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            continue;
        }
        if (explosions[i].blend != 1) {
            explosions[i].y -= 60;
            if (!(partpos & 1)) {
                explosions[i].life--;
            }
        } else {
            explosions[i].life = explosions[i].life - 1;
        }
        frame = explosions[i].life;
        if (frame >= 15 || explosions[i].flash != 1) {
            if (frame >= 17 || explosions[i].flash != 0) {
                continue;
            }
            frame = 16 - frame;
        } else {
            frame = 29 - frame;
        }
        if (explosions[i].ship >= 0) {
            relPos.vx = explosions[i].x + camera.vx + shipIndex[explosions[i].ship].ppivot.vx;
            relPos.vy = explosions[i].y + camera.vy + shipIndex[explosions[i].ship].ppivot.vy;
            relPos.vz = explosions[i].z + camera.vz + shipIndex[explosions[i].ship].ppivot.vz;
        } else {
            relPos.vx = explosions[i].x + camera.vx;
            relPos.vy = explosions[i].y + camera.vy;
            relPos.vz = explosions[i].z + camera.vz;
        }
        x = relPos.vx;
        absX = (x >= 0) ? x : -x;
        if (absX > 0x7fff) {
            continue;
        }
        z = relPos.vz;
        absZ = (z >= 0) ? z : -z;
        if (absZ > 0x7fff) {
            continue;
        }
        vertex.vx = x;
        vertex.vz = z;
        vertex.vy = relPos.vy;
        depth = RotTransPers(&vertex, (int *)&sxy, &p, &flag);
        left = sxy.vx;
        top = sxy.vy;
        if (flag & 0x80460000) {
            continue;
        }
        size = explosions[i].size;
        radius = (0x19000 >> size) / depth;
        maxSize = 480 >> size;
        if (radius > maxSize) {
            radius = maxSize;
        }
        left -= radius >> 1;
        top -= radius >> 1;
        if (explosions[i].blend == 1) {
            depth -= 256;
        }
        if (depth < 0) {
            depth = 0;
        }
        explodePrims[i][CurrentScreen].x0 = left;
        explodePrims[i][CurrentScreen].y0 = top;
        explodePrims[i][CurrentScreen].x1 = left + radius;
        explodePrims[i][CurrentScreen].y1 = top;
        explodePrims[i][CurrentScreen].x2 = left;
        explodePrims[i][CurrentScreen].y2 = top + radius;
        explodePrims[i][CurrentScreen].x3 = left + radius;
        explodePrims[i][CurrentScreen].y3 = top + radius;
        explodePrims[i][CurrentScreen].r0 = explosions[i].r;
        explodePrims[i][CurrentScreen].g0 = explosions[i].g;
        explodePrims[i][CurrentScreen].b0 = explosions[i].b;
        EXPLODE_QUAD(i)->u0 = explodeTexture[frame]->u0;
        EXPLODE_QUAD(i)->v0 = explodeTexture[frame]->v0;
        EXPLODE_QUAD(i)->u1 = explodeTexture[frame]->u1;
        EXPLODE_QUAD(i)->v1 = explodeTexture[frame]->v1;
        EXPLODE_QUAD(i)->u2 = explodeTexture[frame]->u2;
        EXPLODE_QUAD(i)->v2 = explodeTexture[frame]->v2;
        EXPLODE_QUAD(i)->u3 = explodeTexture[frame]->u3;
        EXPLODE_QUAD(i)->v3 = explodeTexture[frame]->v3;
        EXPLODE_QUAD(i)->tpage = GetTPage(1, explosions[i].blend, explodeTexture[frame]->textureX, explodeTexture[frame]->textureY);
        explodePrims[i][CurrentScreen].clut = explodeTexture[frame]->cba;
        AddPrim(OT[CurrentScreen] + (depth >> 1), EXPLODE_QUAD(i));
    }
}


void DrawBirds(void)
{
    SVECTOR  vertex;
    VECTOR   relPos;
    DVECTOR  sxy;
    VECTOR   camera;
    int     p, flag;
    int      n, frame, depth, page;
    short    size;
    int      absX, absZ;
    int      x, y, z, tx, ty, tz, sx, sy, half;
    Bird     *bird;
    POLY_FT4 *prim, *quad;

    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    for (n = 0; n < BIRD_COUNT; n++) {
        frame = birds[n].frame;
        if (frame != 8 || birds[n].vx <= 0) {
            birds[n].frame = (frame + 2) & 31;
        }
        if (n < 15) {
            tx = birdSectionA->centre.vx + 512;
            ty = birdSectionA->centre.vy - 7400;
            tz = birdSectionA->centre.vz - 512;
        } else {
            tx = birdSectionB->centre.vx + 512;
            ty = birdSectionB->centre.vy - 7400;
            tz = birdSectionB->centre.vz - 512;
        }
        if (tx < birds[n].x) {
            birds[n].vx -= birds[n].accel;
        }
        if (tx > birds[n].x) {
            birds[n].vx += birds[n].accel;
        }
        if (ty < birds[n].y) {
            birds[n].vy -= birds[n].accel * 2;
        }
        if (ty > birds[n].y) {
            birds[n].vy += birds[n].accel * 2;
        }
        if (tz < birds[n].z) {
            birds[n].vz -= birds[n].accel;
        }
        if (tz > birds[n].z) {
            birds[n].vz += birds[n].accel;
        }
        if (birds[n].vx > 320) {
            birds[n].vx = 320;
        }
        if (birds[n].vx < -320) {
            birds[n].vx = -320;
        }
        if (birds[n].vy > 320) {
            birds[n].vy = 160;
        }
        if (birds[n].vy < -320) {
            birds[n].vy = -160;
        }
        if (birds[n].vz > 320) {
            birds[n].vz = 320;
        }
        if (birds[n].vz < -320) {
            birds[n].vz = -320;
        }
        bird = (Bird *)(n * sizeof(Bird) + (int)birds);
        bird->x += bird->vx;
        bird->y += bird->vy;
        bird->z += bird->vz;
        x = bird->x + camera.vx;
        relPos.vx = x;
        y = bird->y + camera.vy;
        relPos.vy = y;
        z = bird->z + camera.vz;
        absX = (x >= 0) ? x : -x;
        relPos.vz = z;
        if (absX > 0x7fff) {
            continue;
        }
        absZ = (z >= 0) ? z : -z;
        if (absZ > 0x7fff) {
            continue;
        }
        vertex.vx = x;
        vertex.vy = y;
        vertex.vz = z;
        depth = RotTransPers(&vertex, (int *)&sxy, &p, &flag);
        sx = sxy.vx;
        sy = sxy.vy;
        if (flag & 0x80460000) {
            continue;
        }
        size = 51200 / depth;
        if (size > 280) {
            size = 280;
        }
        prim = (POLY_FT4 *)((n * 2 + CurrentScreen) * sizeof(POLY_FT4) + (int)birdPrims);
        prim->r0 = 96;
        half = size >> 1;
        sx -= half;
        sy -= half;
        prim->x0 = sx;
        prim->y0 = sy;
        prim->x1 = size + sx;
        prim->y1 = sy;
        prim->x2 = sx;
        prim->y2 = size + sy;
        prim->x3 = size + sx;
        prim->y3 = size + sy;
        birdPrims[n * 2 + CurrentScreen].g0 = 96;
        birdPrims[n * 2 + CurrentScreen].b0 = 96;
        birdPrims[n * 2 + CurrentScreen].u0 = birdTexture[frame]->u0;
        birdPrims[n * 2 + CurrentScreen].v0 = birdTexture[frame]->v0;
        birdPrims[n * 2 + CurrentScreen].u1 = birdTexture[frame]->u1;
        birdPrims[n * 2 + CurrentScreen].v1 = birdTexture[frame]->v1;
        birdPrims[n * 2 + CurrentScreen].u2 = birdTexture[frame]->u2;
        birdPrims[n * 2 + CurrentScreen].v2 = birdTexture[frame]->v2;
        birdPrims[n * 2 + CurrentScreen].u3 = birdTexture[frame]->u3;
        birdPrims[n * 2 + CurrentScreen].v3 = birdTexture[frame]->v3;
        page = GetTPage(0, 0, birdTexture[frame]->textureX, birdTexture[frame]->textureY);
        quad = (POLY_FT4 *)((n * 2 + CurrentScreen) * sizeof(POLY_FT4) + (int)birdPrims);
        quad->tpage = page;
        quad->clut = birdTexture[frame]->cba;
        AddPrim(OT[CurrentScreen] + (depth >> 1), quad);
    }
}


void InitRing(void)
{
    int i;
    int j;
    int k;

    for (i = 0; i < RING_COUNT; i++) {
        rings[i].x = 0;
        rings[i].y = 0;
        rings[i].z = 0;
        rings[i].life = 0;
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 16; k++) {
                SetPolyG4(&ringPrims[j][k][i]);
                ringPrims[j][k][i].r0 = 255;
                ringPrims[j][k][i].g0 = 255;
                ringPrims[j][k][i].b0 = 255;
                ringPrims[j][k][i].r1 = 255;
                ringPrims[j][k][i].g1 = 255;
                ringPrims[j][k][i].b1 = 255;
                ringPrims[j][k][i].r2 = 0;
                ringPrims[j][k][i].g2 = 0;
                ringPrims[j][k][i].b2 = 0;
                ringPrims[j][k][i].r3 = 0;
                ringPrims[j][k][i].g3 = 0;
                ringPrims[j][k][i].b3 = 0;
                SetSemiTrans(&ringPrims[j][k][i], 1);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 4; k++) {
                SetPolyG3(&missileFlarePrims[j][k][i]);
                missileFlarePrims[j][k][i].r2 = 32;
                missileFlarePrims[j][k][i].g2 = 64;
                missileFlarePrims[j][k][i].r0 = 0;
                missileFlarePrims[j][k][i].g0 = 0;
                missileFlarePrims[j][k][i].b0 = 0;
                missileFlarePrims[j][k][i].r1 = 0;
                missileFlarePrims[j][k][i].g1 = 0;
                missileFlarePrims[j][k][i].b1 = 0;
                missileFlarePrims[j][k][i].b2 = 255;
                SetSemiTrans(&missileFlarePrims[j][k][i], 1);
            }
        }
    }
    for (i = 0; i < 15; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 4; k++) {
                SetPolyG3(&rocketFlarePrims[j][k][i]);
                rocketFlarePrims[j][k][i].r2 = 255;
                rocketFlarePrims[j][k][i].g2 = 64;
                rocketFlarePrims[j][k][i].r0 = 0;
                rocketFlarePrims[j][k][i].g0 = 0;
                rocketFlarePrims[j][k][i].b0 = 0;
                rocketFlarePrims[j][k][i].r1 = 0;
                rocketFlarePrims[j][k][i].g1 = 0;
                rocketFlarePrims[j][k][i].b1 = 0;
                rocketFlarePrims[j][k][i].b2 = 32;
                SetSemiTrans(&rocketFlarePrims[j][k][i], 1);
            }
        }
    }
}


void DrawRing(void)
{
    SVECTOR vertex;
    DVECTOR sxy;
    int p, flag;
    VECTOR camera;
    int i, k, sx, sy, depth, x, z, age, shade;
    short scale, outer, inner;
    POLY_G4 *prim;
    int sin0, cos0, sin1, cos1;

    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    pitsparkat = (pitsparkat + 160) & 4095;
    exdelay = (exdelay - 96) & 4095;
    SetSkeletonTransform(cameraPtr->camPos->super);
    for (i = 0; i < RING_COUNT; i++) {
        if (rings[i].life <= 0) continue;
        rings[i].life--;
        rings[i].y -= 15;
        if (rings[i].life >= 17) continue;
        age = 16 - rings[i].life;
        vertex.vx = rings[i].x + camera.vx;
        vertex.vy = rings[i].y + camera.vy;
        vertex.vz = rings[i].z + camera.vz;
        x = rings[i].x + camera.vx;
        x = (x >= 0) ? x : -x;
        if (x > 32767) continue;
        z = rings[i].z + camera.vz;
        z = (z >= 0) ? z : -z;
        if (z > 32767) continue;
        depth = RotTransPers(&vertex, (int *)&sxy, &p, &flag);
        sx = sxy.vx;
        sy = sxy.vy;
        if (flag & 0x80460000) continue;
        scale = 51200 / depth;
        if (scale > 256) scale = 256;
        outer = (scale * age) >> 1;
        shade = (16 - age) * 16;
        if (shade > 255) shade = 255;
        inner = outer >> 1;
        for (k = 0; k < 16; k++) {
            sin0 = rsin(k * 256);
            cos0 = rcos(k * 256);
            sin1 = rsin((k + 1) * 256);
            cos1 = rcos((k + 1) * 256);
            prim = &ringPrims[CurrentScreen][k][i];
            prim->r0 = shade;
            prim->r1 = shade;
            prim->g0 = shade;
            prim->g1 = shade;
            prim->b0 = shade;
            prim->b1 = shade;
            prim->x0 = sx + ((outer * sin0) >> 12);
            prim->y0 = sy + ((outer * cos0) >> 12);
            prim->x1 = sx + ((outer * sin1) >> 12);
            prim->y1 = sy + ((outer * cos1) >> 12);
            prim->x2 = sx + ((inner * sin0) >> 12);
            prim->y2 = sy + ((inner * cos0) >> 12);
            prim->x3 = sx + ((inner * sin1) >> 12);
            prim->y3 = sy + ((inner * cos1) >> 12);
            AddPrim(OT[CurrentScreen] + (depth >> 1), &ringPrims[CurrentScreen][k][i]);
        }
    }
}


void AddSpurtExplode(int x, int y, int z, short ship)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 15;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 128;
            explosions[i].g = 128;
            explosions[i].b = 128;
            explosions[i].blend = 1;
            explosions[i].flash = 1;
            explosions[i].ship = ship;
            explosions[i].size = 1;
            return;
        }
    }
}


void AddFireExplode(int x, int y, int z, short ship)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 80;
            explosions[i].g = 80;
            explosions[i].b = 80;
            explosions[i].blend = 1;
            explosions[i].flash = 1;
            explosions[i].ship = ship;
            explosions[i].size = 2;
            return;
        }
    }
}


void AddExplode(int x, int y, int z, short ship)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 128;
            explosions[i].g = 128;
            explosions[i].b = 128;
            explosions[i].blend = 1;
            explosions[i].flash = 0;
            explosions[i].ship = ship;
            explosions[i].size = 1;
            return;
        }
    }
}


void AddTendrilExplode(int x, int y, int z)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 128;
            explosions[i].g = 96;
            explosions[i].b = 32;
            explosions[i].blend = 1;
            explosions[i].ship = -1;
            explosions[i].flash = 1;
            explosions[i].size = 2;
            return;
        }
    }
}


void AddMinigunExplode(int x, int y, int z, int unused)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 32;
            explosions[i].g = 64;
            explosions[i].b = 192;
            explosions[i].blend = 1;
            explosions[i].ship = -1;
            explosions[i].flash = 1;
            explosions[i].size = 2;
            return;
        }
    }
}


void AddSmallSpurtExplode(int x, int y, int z, short ship)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 96;
            explosions[i].g = 96;
            explosions[i].b = 96;
            explosions[i].blend = 1;
            explosions[i].ship = ship;
            explosions[i].flash = 1;
            explosions[i].size = 3;
            return;
        }
    }
}


void AddSmallSmokeExplode(int x, int y, int z, short ship)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 16;
            explosions[i].g = 16;
            explosions[i].b = 32;
            explosions[i].blend = 2;
            explosions[i].ship = ship;
            explosions[i].flash = 0;
            explosions[i].size = 2;
            return;
        }
    }
}


void AddSmokeExplode(int x, int y, int z, int unused)
{
    int i;

    for (i = 0; i < EXPLOSION_COUNT; i++) {
        if (explosions[i].life <= 0) {
            explosions[i].life = 17;
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].z = z;
            explosions[i].r = 16;
            explosions[i].g = 16;
            explosions[i].b = 32;
            explosions[i].blend = 2;
            explosions[i].flash = 0;
            explosions[i].ship = -1;
            explosions[i].size = 1;
            return;
        }
    }
}


void AddRing(int x, int y, int z, int unused)
{
    int i;

    for (i = 0; i < RING_COUNT; i++) {
        if (rings[i].life <= 0) {
            rings[i].life = 16;
            rings[i].x = x;
            rings[i].y = y;
            rings[i].z = z;
            return;
        }
    }
}


extern POLY_G3 missileFlarePrims[2][4][8];

void DrawMissileFlare(int xpos, int ypos, int zpos)
{
    SVECTOR vertex;
    DVECTOR sxy;
    int p, flag;
    VECTOR camera;
    int savedAngle;
    int pass;
    int depth, x, y, z, extent, trig, product, sx, sy;
    int absZ;
    short radius, full, half;

    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    savedAngle = pitsparkat;
    for (pass = 0; pass < 2; pass++, pitsparkat += 256) {
        missflareat++;
        if (missflareat >= 8) return;
        x = xpos + camera.vx;
        y = ypos + camera.vy;
        z = zpos + camera.vz;
        vertex.vx = x;
        x = (x >= 0) ? x : -x;
        vertex.vy = y;
        vertex.vz = z;
        if (x > 32767) continue;
        absZ = (z >= 0) ? z : -z;
        if (absZ > 32767) continue;
        depth = RotTransPers(&vertex, (int *)&sxy, &p, &flag);
        sx = sxy.vx;
        sy = sxy.vy;
        if (flag & 0x80460000) continue;
        extent = (short)(51200 / depth) >> 1;
        radius = extent;
        if (extent > 256) radius = 256;
        full = radius;
        half = radius >> 1;
        trig = rsin(pitsparkat + 0);
        product = full * trig;
        missileFlarePrims[CurrentScreen][0][missflareat].x0 = sx + (product >> 12);
        trig = rcos(pitsparkat + 0);
        product = full * trig;
        missileFlarePrims[CurrentScreen][0][missflareat].y0 = sy + (product >> 12);
        trig = rsin(pitsparkat + 512);
        product = full * trig;
        missileFlarePrims[CurrentScreen][0][missflareat].x1 = sx + (product >> 12);
        trig = rcos(pitsparkat + 512);
        missileFlarePrims[CurrentScreen][0][missflareat].x2 = sx;
        missileFlarePrims[CurrentScreen][0][missflareat].y2 = sy;
        product = full * trig;
        missileFlarePrims[CurrentScreen][0][missflareat].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &missileFlarePrims[CurrentScreen][0][missflareat]);
        trig = rsin(pitsparkat + 1024);
        product = half * trig;
        missileFlarePrims[CurrentScreen][1][missflareat].x0 = sx + (product >> 12);
        trig = rcos(pitsparkat + 1024);
        product = half * trig;
        missileFlarePrims[CurrentScreen][1][missflareat].y0 = sy + (product >> 12);
        trig = rsin(pitsparkat + 1536);
        product = half * trig;
        missileFlarePrims[CurrentScreen][1][missflareat].x1 = sx + (product >> 12);
        trig = rcos(pitsparkat + 1536);
        missileFlarePrims[CurrentScreen][1][missflareat].x2 = sx;
        missileFlarePrims[CurrentScreen][1][missflareat].y2 = sy;
        product = half * trig;
        missileFlarePrims[CurrentScreen][1][missflareat].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &missileFlarePrims[CurrentScreen][1][missflareat]);
        trig = rsin(pitsparkat + 2048);
        product = full * trig;
        missileFlarePrims[CurrentScreen][2][missflareat].x0 = sx + (product >> 12);
        trig = rcos(pitsparkat + 2048);
        product = full * trig;
        missileFlarePrims[CurrentScreen][2][missflareat].y0 = sy + (product >> 12);
        trig = rsin(pitsparkat + 2560);
        product = full * trig;
        missileFlarePrims[CurrentScreen][2][missflareat].x1 = sx + (product >> 12);
        trig = rcos(pitsparkat + 2560);
        missileFlarePrims[CurrentScreen][2][missflareat].x2 = sx;
        missileFlarePrims[CurrentScreen][2][missflareat].y2 = sy;
        product = full * trig;
        missileFlarePrims[CurrentScreen][2][missflareat].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &missileFlarePrims[CurrentScreen][2][missflareat]);
        trig = rsin(pitsparkat + 3072);
        product = half * trig;
        missileFlarePrims[CurrentScreen][3][missflareat].x0 = sx + (product >> 12);
        trig = rcos(pitsparkat + 3072);
        product = half * trig;
        missileFlarePrims[CurrentScreen][3][missflareat].y0 = sy + (product >> 12);
        trig = rsin(pitsparkat + 3584);
        product = half * trig;
        missileFlarePrims[CurrentScreen][3][missflareat].x1 = sx + (product >> 12);
        trig = rcos(pitsparkat + 3584);
        missileFlarePrims[CurrentScreen][3][missflareat].x2 = sx;
        missileFlarePrims[CurrentScreen][3][missflareat].y2 = sy;
        product = half * trig;
        missileFlarePrims[CurrentScreen][3][missflareat].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &missileFlarePrims[CurrentScreen][3][missflareat]);
    }
    pitsparkat = savedAngle;
}


extern POLY_G3 rocketFlarePrims[2][4][15];

void DrawRocketFlare(int xpos, int ypos, int zpos)
{
    SVECTOR vertex;
    DVECTOR sxy;
    int p, flag;
    VECTOR camera;
    int savedAngle;
    int pass;
    int depth, x, y, z, extent, trig, product, sx, sy;
    int absZ;
    short radius, full, half;

    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    savedAngle = exdelay;
    for (pass = 0; pass < 2; pass++, exdelay += 256) {
        rockang++;
        if (rockang >= 15) return;
        x = xpos + camera.vx;
        y = ypos + camera.vy;
        z = zpos + camera.vz;
        vertex.vx = x;
        x = (x >= 0) ? x : -x;
        vertex.vy = y;
        vertex.vz = z;
        if (x > 32767) continue;
        absZ = (z >= 0) ? z : -z;
        if (absZ > 32767) continue;
        depth = RotTransPers(&vertex, (int *)&sxy, &p, &flag);
        sx = sxy.vx;
        sy = sxy.vy;
        if (flag & 0x80460000) continue;
        extent = (short)(51200 / depth) >> 1;
        radius = extent;
        if (extent > 256) radius = 256;
        full = radius;
        half = radius >> 1;
        trig = rsin(exdelay + 0);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][0][rockang].x0 = sx + (product >> 12);
        trig = rcos(exdelay + 0);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][0][rockang].y0 = sy + (product >> 12);
        trig = rsin(exdelay + 512);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][0][rockang].x1 = sx + (product >> 12);
        trig = rcos(exdelay + 512);
        rocketFlarePrims[CurrentScreen][0][rockang].x2 = sx;
        rocketFlarePrims[CurrentScreen][0][rockang].y2 = sy;
        product = full * trig;
        rocketFlarePrims[CurrentScreen][0][rockang].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &rocketFlarePrims[CurrentScreen][0][rockang]);
        trig = rsin(exdelay + 1024);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][1][rockang].x0 = sx + (product >> 12);
        trig = rcos(exdelay + 1024);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][1][rockang].y0 = sy + (product >> 12);
        trig = rsin(exdelay + 1536);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][1][rockang].x1 = sx + (product >> 12);
        trig = rcos(exdelay + 1536);
        rocketFlarePrims[CurrentScreen][1][rockang].x2 = sx;
        rocketFlarePrims[CurrentScreen][1][rockang].y2 = sy;
        product = half * trig;
        rocketFlarePrims[CurrentScreen][1][rockang].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &rocketFlarePrims[CurrentScreen][1][rockang]);
        trig = rsin(exdelay + 2048);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][2][rockang].x0 = sx + (product >> 12);
        trig = rcos(exdelay + 2048);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][2][rockang].y0 = sy + (product >> 12);
        trig = rsin(exdelay + 2560);
        product = full * trig;
        rocketFlarePrims[CurrentScreen][2][rockang].x1 = sx + (product >> 12);
        trig = rcos(exdelay + 2560);
        rocketFlarePrims[CurrentScreen][2][rockang].x2 = sx;
        rocketFlarePrims[CurrentScreen][2][rockang].y2 = sy;
        product = full * trig;
        rocketFlarePrims[CurrentScreen][2][rockang].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &rocketFlarePrims[CurrentScreen][2][rockang]);
        trig = rsin(exdelay + 3072);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][3][rockang].x0 = sx + (product >> 12);
        trig = rcos(exdelay + 3072);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][3][rockang].y0 = sy + (product >> 12);
        trig = rsin(exdelay + 3584);
        product = half * trig;
        rocketFlarePrims[CurrentScreen][3][rockang].x1 = sx + (product >> 12);
        trig = rcos(exdelay + 3584);
        rocketFlarePrims[CurrentScreen][3][rockang].x2 = sx;
        rocketFlarePrims[CurrentScreen][3][rockang].y2 = sy;
        product = half * trig;
        rocketFlarePrims[CurrentScreen][3][rockang].y1 = sy + (product >> 12);
        AddPrim(OT[CurrentScreen] + (depth >> 1), &rocketFlarePrims[CurrentScreen][3][rockang]);
    }
    exdelay = savedAngle;
}


void InitElectric(void)
{
    int i;
    int j;

    for (i = 0; i < ELECTRIC_COUNT; i++) {
        for (j = 0; j < 2; j++) {
            SetLineG2(&electrics[i].line1[j]);
            SetLineG2(&electrics[i].line2[j]);
            SetLineG2(&electrics[i].line3[j]);
        }
        electrics[i].life = -1;
    }
}


void AddElectric(VECTOR *position, int mode)
{
    VECTOR camera, startPos, endPos, midPos;
    SVECTOR vertex;
    SVECTOR screen[4];
    int p, flag;
    u_char blue0;
    int index, depth, x, y, z, absX, absZ;
    int red0, green0, red, green, blue;

    rockflareat = rockflareat + 1;
    if (rockflareat >= 32) return;
    index = rockflareat;
    if (mode == 0) {
        electrics[index].start.vx = (newRand() & 255) - 128;
        electrics[index].start.vy = (newRand() & 255) - 128;
        electrics[index].start.vz = (newRand() & 255) - 128;
        electrics[index].end.vx = electrics[index].start.vx * 4 + position->vx;
        electrics[index].end.vy = electrics[index].start.vy * 4 + position->vy;
        electrics[index].end.vz = electrics[index].start.vz * 4 + position->vz;
        electrics[index].start.vx += position->vx;
        electrics[index].start.vy += position->vy;
        electrics[index].start.vz += position->vz;
    } else {
        electrics[index].start.vx = newRand() % 480 - 240;
        electrics[index].start.vy = position->vy;
        electrics[index].start.vz = newRand() % 480 - 240;
        electrics[index].end.vx = electrics[index].start.vx * 4 + position->vx;
        electrics[index].end.vy = position->vy + 320;
        electrics[index].end.vz = electrics[index].start.vz * 4 + position->vz;
        electrics[index].start.vx += position->vx;
        electrics[index].start.vz += position->vz;
    }
    red0 = 0;
    if (mode == 2) {
        red0 = 192;
        green0 = 0;
        blue0 = 0;
        red = 255;
        green = 250;
        blue = 0;
    } else {
        green0 = 0;
        blue0 = 255;
        red = 255;
        green = 255;
        blue = 255;
    }
    camera.vx = cameraPtr->camPos->relative.t[0];
    camera.vy = cameraPtr->camPos->relative.t[1];
    camera.vz = cameraPtr->camPos->relative.t[2];
    cameraPtr->camPos->super->update = 1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    x = electrics[index].start.vx + camera.vx;
    startPos.vx = x;
    y = electrics[index].start.vy + camera.vy;
    startPos.vy = y;
    z = electrics[index].start.vz + camera.vz;
    absX = (x >= 0) ? x : -x;
    startPos.vz = z;
    if (absX > 0x7fff) return;
    absZ = (z >= 0) ? z : -z;
    if (absZ > 0x7fff) return;
    vertex.vx = x;
    vertex.vy = y;
    vertex.vz = z;
    RotTransPers(&vertex, (int *)&screen[0], &p, &flag);
    if (flag & 0x80460000) return;
    endPos.vx = electrics[index].end.vx + camera.vx;
    endPos.vy = electrics[index].end.vy + camera.vy;
    endPos.vz = electrics[index].end.vz + camera.vz;
    electrics[index].life = -1;
    vertex.vx = endPos.vx;
    vertex.vy = endPos.vy;
    vertex.vz = endPos.vz;
    RotTransPers(&vertex, (int *)&screen[1], &p, &flag);
    midPos.vx = (startPos.vx + endPos.vx) >> 1;
    midPos.vy = (startPos.vy + endPos.vy) >> 1;
    midPos.vz = (startPos.vz + endPos.vz) >> 1;
    midPos.vx += (newRand() & 127) - 63;
    midPos.vy += (newRand() & 127) - 63;
    midPos.vz += (newRand() & 127) - 63;
    vertex.vx = midPos.vx;
    vertex.vy = midPos.vy;
    vertex.vz = midPos.vz;
    RotTransPers(&vertex, (int *)&screen[2], &p, &flag);
    midPos.vx = (startPos.vx + endPos.vx) >> 1;
    midPos.vy = (startPos.vy + endPos.vy) >> 1;
    midPos.vz = (startPos.vz + endPos.vz) >> 1;
    midPos.vx += (newRand() & 127) - 63;
    midPos.vy += (newRand() & 127) - 63;
    midPos.vz += (newRand() & 127) - 63;
    vertex.vx = midPos.vx;
    vertex.vy = midPos.vy;
    vertex.vz = midPos.vz;
    depth = RotTransPers(&vertex, (int *)&screen[3], &p, &flag);
    if (flag & 0x80460000) return;
    electrics[index].line1[CurrentScreen].x0 = screen[0].vx;
    electrics[index].line1[CurrentScreen].y0 = screen[0].vy;
    electrics[index].line1[CurrentScreen].x1 = screen[2].vx;
    electrics[index].line1[CurrentScreen].y1 = screen[2].vy;
    electrics[index].line1[CurrentScreen].r0 = red0;
    electrics[index].line1[CurrentScreen].g0 = green0;
    electrics[index].line1[CurrentScreen].b0 = blue0;
    electrics[index].line1[CurrentScreen].r1 = red;
    electrics[index].line1[CurrentScreen].g1 = green;
    electrics[index].line1[CurrentScreen].b1 = blue;
    electrics[index].line2[CurrentScreen].x0 = screen[2].vx;
    electrics[index].line2[CurrentScreen].y0 = screen[2].vy;
    electrics[index].line2[CurrentScreen].x1 = screen[3].vx;
    electrics[index].line2[CurrentScreen].y1 = screen[3].vy;
    electrics[index].line2[CurrentScreen].r0 = red;
    electrics[index].line2[CurrentScreen].g0 = green;
    electrics[index].line2[CurrentScreen].b0 = blue;
    electrics[index].line2[CurrentScreen].r1 = red;
    electrics[index].line2[CurrentScreen].g1 = green;
    electrics[index].line2[CurrentScreen].b1 = blue;
    electrics[index].line3[CurrentScreen].x0 = screen[1].vx;
    electrics[index].line3[CurrentScreen].y0 = screen[1].vy;
    electrics[index].line3[CurrentScreen].x1 = screen[3].vx;
    electrics[index].line3[CurrentScreen].y1 = screen[3].vy;
    electrics[index].line3[CurrentScreen].r0 = red0;
    electrics[index].line3[CurrentScreen].g0 = green0;
    electrics[index].line3[CurrentScreen].b0 = blue0;
    electrics[index].line3[CurrentScreen].r1 = red;
    electrics[index].line3[CurrentScreen].g1 = green;
    electrics[index].line3[CurrentScreen].b1 = blue;
    depth -= 180;
    if (depth < 0) depth = 0;
    AddPrim(OT[CurrentScreen] + (depth >> 1), &electrics[index].line1[CurrentScreen]);
    AddPrim(OT[CurrentScreen] + (depth >> 1), &electrics[index].line2[CurrentScreen]);
    AddPrim(OT[CurrentScreen] + (depth >> 1), &electrics[index].line3[CurrentScreen]);
}


void resetMiscFx(void)
{
    rockflareat = 0;
    rockang = 0;
    missflareat = 0;
}


void UnsetParticles(SParticle *particles)
{
    int i;

    for (i = 0; i < PARTICLE_COUNT; i++) {
        particles[i].flags = 0;
    }
}
