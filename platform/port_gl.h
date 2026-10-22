/* OpenGL as each platform has it. The desktop's OpenGL 3.3 and the web's WebGL 2 come through SDL's headers, the
   functions beyond OpenGL 1.1 looked up by name (gpu_gl.c); iOS and Android have OpenGL ES 3 in a library of
   their own, whose headers declare every function and which the program links (PORT_GL_LINKED). */
#ifndef PORT_GL_H
#define PORT_GL_H

#include <SDL3/SDL.h>

#if defined(SDL_PLATFORM_IOS)
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
