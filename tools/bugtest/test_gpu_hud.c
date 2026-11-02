/* Run with an actual OpenGL context; no game settings, disc or controllers. */
#include "../../platform/psx/gpu_gl.c"
#include <assert.h>
#include <stdio.h>
#include <math.h>
PortConfig portConfig;
PortSettings portSettings;
uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH],psxVramHigh[VRAM_HEIGHT][VRAM_WIDTH];
int portPal;
void PortDrawableSize(int *w,int*h){*w=320;*h=240;}
void HdTexSetScale(int n){}
int HdTexActive(void){return 0;}
unsigned HdTexEpoch(void){return 0;}
int HdTexImageCount(void){return 0;}
const uint8_t *HdTexImage(int i,int*w,int*h){return NULL;}
int HdTexFind(int a,int b,int c,int d,int e,float f,float g,float h,float i,HdTexHit*j){return 0;}
void HdTexChanged(int x,int y,int w,int h){}
void HdTexOverwritten(int x,int y,int w,int h){}
int main(void){
 setbuf(stdout, NULL);
 assert(SDL_Init(SDL_INIT_VIDEO));
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 SDL_Window* win=SDL_CreateWindow("blend probe",320,240,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
 assert(win);SDL_GLContext ctx=SDL_GL_CreateContext(win);assert(ctx);
 portSettings.renderScale=1;portSettings.consoleDepth=1;assert(GpuInit());
 GpuSetDrawArea(0,0,320,240,0,0);
 GpuVertex v[3]={{.x=0,.y=0,.r=128,.a=255},{.x=20,.y=0,.r=128,.a=255},{.x=0,.y=20,.r=128,.a=255}};
 GpuDrawState s={.semi=1};
 for(int bg=0;bg<256;bg+=51){
  GpuFill(0,0,20,20,bg,bg,bg);GpuTriangles(&s,v,3);GpuFlush();
  int w,h;uint8_t*p=GpuReadArea(2,2,1,1,&w,&h);
  assert(abs(p[0]-(bg+128>255?255:bg+128))<=1&&p[1]==bg&&p[2]==bg);
 }

 const unsigned texels[]={0,0x8010,0x0010,0x8000};
 for(int mode=0;mode<4;mode++) for(int k=0;k<4;k++) {
  GpuFill(0,0,20,20,96,96,96);
  psxVram[0][512]=texels[k];GpuVramChanged(512,0,1,1);
  s.textured=1;s.raw=1;s.depth=2;s.pageX=512;s.semi=mode;
  GpuTriangles(&s,v,3);GpuFlush();
  int w,h;uint8_t*p=GpuReadArea(2,2,1,1,&w,&h);
  int r=96,g=96,b=96;
  if(k==2) {r=128;g=b=0;}
  if(k==1 || k==3) {
   int fg=k==1?128:0;
   if(mode==0) {r=48+fg/2;g=b=48;}
   if(mode==1) {r=96+fg;}
   if(mode==2) {r=96-fg;if(r<0)r=0;}
   if(mode==3) {r=96+fg/4;}
  }
  assert(abs(p[0]-r)<=1&&abs(p[1]-g)<=1&&abs(p[2]-b)<=1);
 }

 for(int depth=0;depth<2;depth++) {
  GpuFill(0,0,20,20,96,96,96);
  psxVram[0][512]=1;psxVram[0][769]=0x8010;
  GpuVramChanged(512,0,1,1);GpuVramChanged(769,0,1,1);
  s.depth=depth;s.clutX=768;s.clutY=0;s.semi=1;
  GpuTriangles(&s,v,3);GpuFlush();
  int w,h;uint8_t*p=GpuReadArea(2,2,1,1,&w,&h);
  assert(p[0]==224&&p[1]==96&&p[2]==96);
 }

 s.depth=2;s.pageX=512;s.hudColourOverlay=1;s.semi=1;
 for(int bg=0;bg<=255;bg+=85) for(int black=0;black<2;black++) {
  GpuFill(0,0,20,20,bg,bg,bg);
  psxVram[0][512]=black?0x8000:0x8010;GpuVramChanged(512,0,1,1);
  GpuTriangles(&s,v,3);GpuFlush();
  int w,h;uint8_t*p=GpuReadArea(2,2,1,1,&w,&h);
  int expectedR=black?bg:SDL_min(255,128+(int)lround(bg*0.65));
  int expectedGB=black?bg:(int)lround(bg*0.65);
  assert(abs(p[0]-expectedR)<=1&&abs(p[1]-expectedGB)<=1&&abs(p[2]-expectedGB)<=1);
 }
 /* Scene and HUD triangles can share a batch; the flag belongs to each primitive. */
 GpuFill(0,0,48,20,96,96,96);
 psxVram[0][512]=0x8010;GpuVramChanged(512,0,1,1);
 s.hudColourOverlay=0;GpuTriangles(&s,v,3);
 for(int i=0;i<3;i++) v[i].x+=24;
 s.hudColourOverlay=1;GpuTriangles(&s,v,3);
 int w,h;uint8_t *pixel=GpuReadArea(2,2,1,1,&w,&h);
 assert(pixel[0]==224&&pixel[1]==96&&pixel[2]==96);
 pixel=GpuReadArea(26,2,1,1,&w,&h);
 assert(abs(pixel[0]-190)<=1&&abs(pixel[1]-62)<=1&&abs(pixel[2]-62)<=1);
 /* A track pattern must remain visible through the fill. Transparent gaps and
    the coloured edge must occupy exactly the same texels as in additive mode. */
 uint8_t samples[2][8][3];
 for(int overlay=0;overlay<2;overlay++) {
  GpuFill(0,0,16,8,80,80,80);GpuFill(8,0,8,8,200,200,200);
  for(int x=0;x<16;x++) psxVram[0][512+x]=(x==3||x==11)?0x8000:0x8010;
  GpuVramChanged(512,0,16,1);
  GpuVertex strip[6]={
   {.x=0,.y=0,.u=0,.r=128,.g=128,.b=128,.a=255},
   {.x=16,.y=0,.u=16,.r=128,.g=128,.b=128,.a=255},
   {.x=0,.y=8,.u=0,.r=128,.g=128,.b=128,.a=255},
   {.x=16,.y=0,.u=16,.r=128,.g=128,.b=128,.a=255},
   {.x=0,.y=8,.u=0,.r=128,.g=128,.b=128,.a=255},
   {.x=16,.y=8,.u=16,.r=128,.g=128,.b=128,.a=255}};
  s.hudColourOverlay=overlay;GpuTriangles(&s,strip,6);
  for(int x=0;x<8;x++) {
   pixel=GpuReadArea(x+2,2,1,1,&w,&h);memcpy(samples[overlay][x],pixel,3);
  }
  pixel=GpuReadArea(11,2,1,1,&w,&h);
  assert(pixel[0]==200&&pixel[1]==200&&pixel[2]==200);
 }
 for(int overlay=0;overlay<2;overlay++) {
  assert(samples[overlay][1][0]==80&&samples[overlay][1][1]==80&&samples[overlay][1][2]==80);
  assert(samples[overlay][0][0]>samples[overlay][0][1]);
  assert(samples[overlay][2][0]>samples[overlay][2][1]);
 }
 /* The background changes by 120. Its contrast through the tint stays above 60%. */
 assert(samples[1][6][1]-samples[1][0][1]>=72);
 puts("HUD colour remains visible, black stays transparent, original scene blend modes and CLUTs preserved: OK");
 SDL_GL_DestroyContext(ctx);SDL_DestroyWindow(win);SDL_Quit();
}
