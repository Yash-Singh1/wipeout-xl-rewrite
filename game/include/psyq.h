#ifndef PSYQ_H
#define PSYQ_H

/* PlayStation runtime library types and prototypes (libgte, libgpu, libetc, libcd, libspu, libapi). */

typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned int  u_long;

/* libgte */

typedef struct {
    short m[3][3];
    int  t[3];
} MATRIX;

typedef struct {
    int vx, vy, vz, pad;
} VECTOR;

typedef struct {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    u_char r, g, b, cd;
} CVECTOR;

typedef struct {
    short vx, vy;
} DVECTOR;

int rsin(int a);
int rcos(int a);
int ratan2(int y, int x);
int SquareRoot0(int a);
int SquareRoot12(int a);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetColorMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);
void SetBackColor(int rbk, int gbk, int bbk);
void SetGeomOffset(int ofx, int ofy);
void SetGeomScreen(int h);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
int RotTransPers(SVECTOR *v0, int *sxy, int *p, int *flag);

/* libgpu */

typedef struct {
    short x, y;
    short w, h;
} RECT;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    short  x1, y1;
    short  x2, y2;
} POLY_F3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    short  x1, y1;
    short  x2, y2;
    short  x3, y3;
} POLY_F4;

typedef struct {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    short   x1, y1;
    u_char  u1, v1;
    u_short tpage;
    short   x2, y2;
    u_char  u2, v2;
    u_short pad1;
} POLY_FT3;

typedef struct {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    short   x1, y1;
    u_char  u1, v1;
    u_short tpage;
    short   x2, y2;
    u_char  u2, v2;
    u_short pad1;
    short   x3, y3;
    u_char  u3, v3;
    u_short pad2;
} POLY_FT4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    u_char r1, g1, b1, pad1;
    short  x1, y1;
    u_char r2, g2, b2, pad2;
    short  x2, y2;
} POLY_G3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    u_char r1, g1, b1, pad1;
    short  x1, y1;
    u_char r2, g2, b2, pad2;
    short  x2, y2;
    u_char r3, g3, b3, pad3;
    short  x3, y3;
} POLY_G4;

typedef struct {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    u_char  r1, g1, b1, p1;
    short   x1, y1;
    u_char  u1, v1;
    u_short tpage;
    u_char  r2, g2, b2, p2;
    short   x2, y2;
    u_char  u2, v2;
    u_short pad2;
} POLY_GT3;

typedef struct {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    u_char  r1, g1, b1, p1;
    short   x1, y1;
    u_char  u1, v1;
    u_short tpage;
    u_char  r2, g2, b2, p2;
    short   x2, y2;
    u_char  u2, v2;
    u_short pad2;
    u_char  r3, g3, b3, p3;
    short   x3, y3;
    u_char  u3, v3;
    u_short pad3;
} POLY_GT4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    short  x1, y1;
} LINE_F2;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    u_char r1, g1, b1, p1;
    short  x1, y1;
} LINE_G2;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    short  w, h;
} TILE;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
} TILE_1;

typedef struct {
    u_long  tag;
    u_char  r0, g0, b0, code;
    short   x0, y0;
    u_char  u0, v0;
    u_short clut;
    short   w, h;
} SPRT;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_MODE;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short  x0, y0;
    short  w, h;
} BLK_FILL;

typedef struct {
    u_long tag;
    u_long code[15];
} DR_ENV;

typedef struct {
    RECT    clip;
    short   ofs[2];
    RECT    tw;
    u_short tpage;
    u_char  dtd;
    u_char  dfe;
    u_char  isbg;
    u_char  r0, g0, b0;
    DR_ENV  dr_env;
} DRAWENV;

typedef struct {
    RECT   disp;
    RECT   screen;
    u_char isinter;
    u_char isrgb24;
    u_char pad0, pad1;
} DISPENV;

void SetPolyF3(POLY_F3 *p);
void SetPolyF4(POLY_F4 *p);
void SetPolyFT3(POLY_FT3 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetPolyG3(POLY_G3 *p);
void SetPolyG4(POLY_G4 *p);
void SetPolyGT3(POLY_GT3 *p);
void SetPolyGT4(POLY_GT4 *p);
void SetLineF2(LINE_F2 *p);
void SetLineG2(LINE_G2 *p);
void SetTile(TILE *p);
void SetSemiTrans(void *p, int abe);
void SetShadeTex(void *p, int tge);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
void AddPrim();
void DrawOTag(u_long *p);
u_long *ClearOTagR(u_long *ot, int n);
int DrawSync(int mode);
int LoadImage(RECT *rect, u_long *p);
int StoreImage(RECT *rect, u_long *p);
u_short GetTPage(int tp, int abr, int x, int y);
u_short GetClut(int x, int y);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
void FntLoad(int tx, int ty);
int FntOpen(int x, int y, int w, int h, int isbg, int n);
void SetDumpFnt(int id);

/* libetc */

int VSync();
int VSyncCallback(void (*f)(void));

/* libcd */

typedef struct {
    u_char minute;
    u_char second;
    u_char sector;
    u_char track;
} CdlLOC;

/* libspu */

typedef struct {
    short left;
    short right;
} SpuVolume;

typedef struct {
    u_long    voice;
    u_long    mask;
    SpuVolume volume;
    SpuVolume volmode;
    SpuVolume volumex;
    u_short   pitch;
    u_short   note;
    u_short   sample_note;
    short     envx;
    u_long    addr;
    u_long    loop_addr;
    int      a_mode;
    int      s_mode;
    int      r_mode;
    u_short   ar;
    u_short   dr;
    u_short   sr;
    u_short   rr;
    u_short   sl;
    u_short   adsr1;
    u_short   adsr2;
} SpuVoiceAttr;

typedef struct {
    SpuVolume volume;
    int      reverb;
    int      mix;
} SpuExtAttr;

typedef struct {
    u_long     mask;
    SpuVolume  mvol;
    SpuVolume  mvolmode;
    SpuVolume  mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

typedef struct {
    u_long    mask;
    int      mode;
    SpuVolume depth;
    int      delay;
    int      feedback;
} SpuReverbAttr;

/* libapi / libcd / libgpu / libspu / libetc (declared without parameters where call sites differ) */
int  CdInit();
int  CdControl();
int  ClearImage(RECT *rect, u_char r, u_char g, u_char b);
int  MoveImage(RECT *rect, int x, int y);
void SetDispMask(int mask);
int  ResetGraph(int mode);
int  EnterCriticalSection(void);
int  ExitCriticalSection(void);
int  Exec();
int  TestEvent(int event);
int GsGetVcount(void);
void GsClearVcount();
int  CdSetDebug(int level);
int  CdControlB(u_char com, u_char *param, u_char *result);
int  CdGetToc(CdlLOC *toc);
void CdIntToPos(int i, void *loc);
int  CdPosToInt(void *loc);
void SpuQuit(void);
void SpuStart(void);
void SpuInit();
int SpuInitMalloc();
int SpuMalloc();
void SpuFree();
u_long SpuSetTransferStartAddr();
u_long SpuWrite();
int SpuSetTransferMode();
int SpuIsTransferCompleted();
void SpuSetVoiceAttr();
void SpuGetVoiceAttr();
void SpuSetKey();
int SpuGetKeyStatus();
void SpuSetCommonAttr();
int SpuSetReverb();
u_long SpuSetReverbVoice();
int SpuSetReverbModeParam();
int SpuReserveReverbWorkArea();
int SpuClearReverbWorkArea();
void StopCallback(void);
int  StopPAD(void);

/* BIOS memory card */
void _card_clear(int port);
void _card_load(int port);
void _card_write(int port, int sector, void *buffer);
void _new_card(void);

/* C library: the standard headers (forced in by platform/include/port.h) declare
   strcat, strcpy, strncmp, strlen, sprintf, printf and the rest.

   Port: the PsyQ BIOS file calls (the memory card and the serial port: libapi) are
   named like the host's POSIX file calls, which a native program also links.  They
   keep their prototypes and get distinct link names. */
int PsyqOpen(char *name, int mode);
int PsyqClose(int fd);
int PsyqRead(int fd, void *buffer, int size);
int PsyqWrite(int fd, void *buffer, int size);
int PsyqRename(char *oldName, char *newName);
#define open(name, mode)         PsyqOpen(name, mode)
#define close(fd)                PsyqClose(fd)
#define read(fd, buffer, size)   PsyqRead(fd, buffer, size)
#define write(fd, buffer, size)  PsyqWrite(fd, buffer, size)
#define rename(oldName, newName) PsyqRename(oldName, newName)

#endif
