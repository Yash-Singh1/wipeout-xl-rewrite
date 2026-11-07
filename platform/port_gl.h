/* Desktop GL entry points are loaded through SDL. WebGL and mobile GLES expose
   linked entry points. WebGL calls go directly to Emscripten instead of through
   SDL procedure-address wrappers and indirect function-table calls. */
#ifndef PORT_GL_H
#define PORT_GL_H

#include <SDL3/SDL.h>

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#define PORT_GL_LINKED 1
#elif defined(SDL_PLATFORM_IOS)
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#define PORT_GL_LINKED 1
#elif defined(SDL_PLATFORM_ANDROID)
#include <GLES3/gl3.h>
#define PORT_GL_LINKED 1
#else
#include <SDL3/SDL_opengl.h>
#endif

#endif
