/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This is a port of the infamous "gears" demo to straight GLX (i.e. no GLUT)
 * Port by Brian Paul  23 March 2001
 *
 * See usage() below for command line options.
 */


#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
#endif

typedef GLXContext (*glXCreateContextAttribsARBProc)
    (Display*, GLXFBConfig, GLXContext, Bool, const int*);

#define BENCHMARK

#ifdef BENCHMARK

/* XXX this probably isn't very portable */

#include <sys/time.h>
#include <unistd.h>

/* return current time (in seconds) */
static double
current_time(void)
{
   struct timeval tv;
#ifdef __VMS
   (void) gettimeofday(&tv, NULL );
#else
   struct timezone tz;
   (void) gettimeofday(&tv, &tz);
#endif
   return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
}

#else /*BENCHMARK*/

/* dummy */
static double
current_time(void)
{
   /* update this function for other platforms! */
   static double t = 0.0;
   static int warn = 1;
   if (warn) {
      printf( "Warning: current_time() not implemented!!\n");
      warn = 0;
   }
   return t += 1.0;
}

#endif /*BENCHMARK*/



#ifndef M_PI
#define M_PI 3.14159265
#endif


/** Event handler results: */
#define NOP 0
#define EXIT 1
#define DRAW 2

static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static GLboolean fullscreen = GL_FALSE;	/* Create a single fullscreen window */
static GLboolean stereo = GL_FALSE;	/* Enable stereo.  */
static GLint samples = 0;               /* Choose visual with at least N samples. */
static GLboolean animate = GL_TRUE;	/* Animation */
static GLfloat eyesep = 5.0;		/* Eye separation. */
static GLfloat fix_point = 40.0;	/* Fixation point distance.  */
static GLfloat left, right, asp;	/* Stereo frustum params.  */


#define GL_DEBUG(m) \
  m;                \
  check_gl_error(#m);

const char* glErrorString(GLenum err) {
  switch (err) {
    case GL_NO_ERROR:
      return "GL_NO_ERROR: No error has been recorded";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM: An unacceptable value is specified for an "
             "enumerated argument";
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION: The specified operation is not allowed in "
             "the current state";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION: The command is trying to "
             "render "
             "to or read from the framebuffer while the currently bound "
             "framebuffer is not framebuffer complete";
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY: There is not enough memory left to execute the "
             "command";
  }
  return "Unspecified Error";
}

// Simple check error call
int check_gl_error(const char* call) {
  int err = glGetError();
  if (err != 0) {
    int prog;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    printf( "%s\nGlError: '%s' CurrentProgram: %i\n\n", call,
            glErrorString(err), prog);
    //exit(1);
  }
  return err;
}

// We can wrap the check error in a define to disable the checking
#define GL_DEBUG(m) \
  m;                \
  check_gl_error(#m);

// TODO Figure out how to get this compound statement to work so it's one r
// value statment. Current breaks on void return values. #define GL_DEBUG(m)
//({__auto_type ret = m; check_gl_error(#m); ret;})

// Compiles a shader
static GLint compile_shader(GLenum shader_type, const char* shader_file) {
  // Open our shader file
  FILE* f;
  f = fopen(shader_file, "r");
  if (!f) {
    printf( "Unable to open shader file %s. Aborting.\n", shader_file);
    return -1;
  }
  // Get the shader size
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  printf( "Compiling %s. Shader size: %li bytes\n", shader_file, fsize);
  char shader_source[fsize + 1];
  fseek(f, 0, SEEK_SET);
  fread(shader_source, fsize, 1, f);
  shader_source[fsize] = '\0';
  fclose(f);
  // Compile the shader
  GLuint shader = GL_DEBUG(glCreateShader(shader_type));
  const char* ss = shader_source;
  glShaderSource(shader, 1, &ss, NULL);
  glCompileShader(shader);
  // Check status
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    // Output error if there was a problem
    GLint info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
    GLchar info_log[info_log_length];
    glGetShaderInfoLog(shader, info_log_length, NULL, info_log);
    printf( "Compile failure in shader:\n%s\n", info_log);
    return -1;
  }
  return shader;
}

// Verticies for our output triangle
//
// We enable culling as an example below, so  the order these
// are provided actually matter. We need, using the right hand
// rule, for the trangle's face to be towards the screen
//
// Recall that screen space coordinates are laid out like the following:
//  0.0  0.0 -> Center
// -1.0 -1.0 -> Bottom left
//  1.0  1.0 -> Top right
static const GLfloat triangle[][2] = {
    {0.0f, 1.0f},    // Top Middle
    {-1.0f, -1.0f},  // Bottom Left
    {1.0f, -1.0f}    // Bottom Right
};

// Color for our output triangle, corresponding to our verticies
// In this case R, G, B, and an Alpha channel
static const GLfloat colors[][4] = {
    {1.0, 0.0, 0.0, 1.0},  // Red
    {0.0, 1.0, 0.0, 1.0},  // Green
    {0.0, 0.0, 1.0, 1.0},  // Blue
};



/** Draw single frame, do SwapBuffers, compute FPS */
static void
draw_frame(Display *dpy, Window win)
{

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Now we draw the triangles. There are 3 points to draw
    GL_DEBUG(glDrawArrays(GL_TRIANGLES, 0, 3));

   glXSwapBuffers(dpy, win);
}

static void
init(void)
{
  // enable depth testing. This doesn't matter for a single triangle but
  // useful if you have a 3d scene or multiple triangles that overlap
  //
  // While you could get away with rendering triangles in the right order,
  // this is difficult to do in complex scenes or scenes where you need
  // per-pixel ordering.
  GL_DEBUG(glEnable(GL_DEPTH_TEST));
  GL_DEBUG(glDepthFunc(GL_LESS));
  GL_DEBUG(glDepthRange(0, 1000));
  // Enable alpha blending
  GL_DEBUG(glEnable(GL_BLEND));
  GL_DEBUG(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  GL_DEBUG(glEnable(GL_MULTISAMPLE));
  // We can cull triangles that are wound away from us. Also important for 3d
  // scenes but not this so much. Note that since we have it enabled here
  // the order we provide the verticies in the triangle array matters
  //
  // The rule that if the triangle is wound facing the viewer it will be shown
  GL_DEBUG(glEnable(GL_CULL_FACE));

  // Compile our shaders
  GLint vertex_shader;
  GLint fragment_shader;
  vertex_shader = compile_shader(GL_VERTEX_SHADER, "vertex.glsl");
  fragment_shader = compile_shader(GL_FRAGMENT_SHADER, "fragment.glsl");
  // If we have a problem quit
  if (vertex_shader < 0 || fragment_shader < 0)
    exit(1);

  // Link the shaders together into a single 'shader program'
  GLuint program;
  program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  // error checking
  GLint status;
  // Get link log if there is a problem
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    GLint info_log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);

    GLchar info_log[info_log_length];
    glGetProgramInfoLog(program, info_log_length, NULL, info_log);
    printf( "Shader linker failure: %s\n", info_log);
    exit(1);
  }
  // Once the shaders are linked into a program they don't need to be kept
  // around
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  // Handles for various buffers
  // vao is a buffer of buffers, (so will point our verticies and colors)
  // vbo_verticies is for the triangle points and
  // vbo_colors is for the triangle colors
  GLuint vao, vbo_verticies, vbo_colors;

  // Generate a single Vertex Array and bind it  (meaning subsequent calls to do
  // with vertex arrays will refer to it)
  GL_DEBUG(glGenVertexArrays(1, &vao));
  GL_DEBUG(glBindVertexArray(vao));
  // Generate the buffer object for the verticies
  GL_DEBUG(glGenBuffers(1, &vbo_verticies));
  // bind it for the next few calls
  GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, vbo_verticies));
  // Upload our triangle data to the vbo
  GL_DEBUG(glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle,
                        GL_STATIC_DRAW));
  // We get the location of the 'in_position' named in the vertex shader
  GLint in_position_loc = GL_DEBUG(glGetAttribLocation(program, "in_position"));
  // Set the location in the vao to this buffer and tell it how to access the
  // data. We have 2 points per vertex hence 2, and sizeof(float) * 2 and the
  // GL_FLOAT

  // Enable this buffer
  // Now geneate the vbo for colors
  GL_DEBUG(glGenBuffers(1, &vbo_colors));
  // Bind it for the next few calls
  GL_DEBUG(glBindBuffer(GL_ARRAY_BUFFER, vbo_colors));
  // Upload the color data in the same way we did triangles
  GL_DEBUG(
      glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW));
  // We get the location of the 'in_color' named in the vertex shader
  GLint in_color_loc = GL_DEBUG(glGetAttribLocation(program, "in_color"));

  // Enable the vbo
  // Now we set to use the shader program we previously compiled
  GL_DEBUG( glUseProgram(program) );
}


/* new window size or exposure */
static void
reshape(int width, int height)
{
   glViewport(0, 0, (GLint) width, (GLint) height);


   GLfloat h = (GLfloat) height / (GLfloat) width;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);

   
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.0, 0.0, -40.0);
}
   





/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void
make_window( Display *dpy, const char *name,
             int x, int y, int width, int height,
             Window *winRet, GLXContext *ctxRet, VisualID *visRet)
{
       Display* disp = 0;
    Window win = 0;

    /* Create_display_and_window
       -------------------------
       Skip if you already have a display and window */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
                              10, 10,   /* x, y */
                              800, 600, /* width, height */
                              0, 0,     /* border_width, border */
                              0);       /* background */

    /* Create_the_modern_OpenGL_context
       -------------------------------- */
    static int visual_attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        None
    };

    int num_fbc = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(dpy,
                                         DefaultScreen(dpy),
                                         visual_attribs, &num_fbc);
    if (!fbc) {
        printf("glXChooseFBConfig() failed\n");
        exit(1);
    }
       
   glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)
        glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

       if (!glXCreateContextAttribsARB) {
        printf("glXCreateContextAttribsARB() not found\n");
        exit(1);
    }

   
/* Set desired minimum OpenGL version */
    static int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        None
    };
   GLXContext ctx = glXCreateContextAttribsARB( dpy, fbc[0], NULL, True,
                                                context_attribs);
                                                
   if (!ctx) {
      printf("Error: glXCreateContext failed\n");
      exit(1);
   }

   

   *winRet = win;
   *ctxRet = ctx;

}


/**
 * Determine whether or not a GLX extension is supported.
 */
static int
is_glx_extension_supported(Display *dpy, const char *query)
{
   const int scrnum = DefaultScreen(dpy);
   const char *glx_extensions = NULL;
   const size_t len = strlen(query);
   const char *ptr;

   if (glx_extensions == NULL) {
      glx_extensions = glXQueryExtensionsString(dpy, scrnum);
   }

   ptr = strstr(glx_extensions, query);
   return ((ptr != NULL) && ((ptr[len] == ' ') || (ptr[len] == '\0')));
}


/**
 * Attempt to determine whether or not the display is synched to vblank.
 */
static void
query_vsync(Display *dpy, GLXDrawable drawable)
{
   int interval = 0;

#if defined(GLX_EXT_swap_control)
   if (is_glx_extension_supported(dpy, "GLX_EXT_swap_control")) {
       unsigned int tmp = -1;
       glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &tmp);
       interval = tmp;
   } else
#endif
   if (is_glx_extension_supported(dpy, "GLX_MESA_swap_control")) {
      PFNGLXGETSWAPINTERVALMESAPROC pglXGetSwapIntervalMESA =
          (PFNGLXGETSWAPINTERVALMESAPROC)
          glXGetProcAddressARB((const GLubyte *) "glXGetSwapIntervalMESA");

      interval = (*pglXGetSwapIntervalMESA)();
   } else if (is_glx_extension_supported(dpy, "GLX_SGI_swap_control")) {
      /* The default swap interval with this extension is 1.  Assume that it
       * is set to the default.
       *
       * Many Mesa-based drivers default to 0, but all of these drivers also
       * export GLX_MESA_swap_control.  In that case, this branch will never
       * be taken, and the correct result should be reported.
       */
      interval = 1;
   }


   if (interval > 0) {
      printf("Running synchronized to the vertical refresh.  The framerate should be\n");
      if (interval == 1) {
         printf("approximately the same as the monitor refresh rate.\n");
      } else if (interval > 1) {
         printf("approximately 1/%d the monitor refresh rate.\n",
                interval);
      }
   }
}

/**
 * Handle one X event.
 * \return NOP, EXIT or DRAW
 */
static int
handle_event(Display *dpy, Window win, XEvent *event)
{
   (void) dpy;
   (void) win;

   switch (event->type) {
   case Expose:
      return DRAW;
   case ConfigureNotify:
      reshape(event->xconfigure.width, event->xconfigure.height);
      break;
   case KeyPress:
      {
         char buffer[10];
         int code;
         code = XLookupKeysym(&event->xkey, 0);
         if (code == XK_Left) {
            view_roty += 5.0;
         }
         else if (code == XK_Right) {
            view_roty -= 5.0;
         }
         else if (code == XK_Up) {
            view_rotx += 5.0;
         }
         else if (code == XK_Down) {
            view_rotx -= 5.0;
         }
         else {
            XLookupString(&event->xkey, buffer, sizeof(buffer),
                          NULL, NULL);
            if (buffer[0] == 27) {
               /* escape */
               return EXIT;
            }
            else if (buffer[0] == 'a' || buffer[0] == 'A') {
               animate = !animate;
            }
         }
         return DRAW;
      }
   }
   return NOP;
}


static void
event_loop(Display *dpy, Window win)
{
   while (1) {
      int op;
      while (!animate || XPending(dpy) > 0) {
         XEvent event;
         XNextEvent(dpy, &event);
         op = handle_event(dpy, win, &event);
         if (op == EXIT)
            return;
         else if (op == DRAW)
            break;
      }

      draw_frame(dpy, win);
   }
}


static void
usage(void)
{
   printf("Usage:\n");
   printf("  -display <displayname>  set the display to run on\n");
   printf("  -stereo                 run in stereo mode\n");
   printf("  -samples N              run in multisample mode with at least N samples\n");
   printf("  -fullscreen             run in fullscreen mode\n");
   printf("  -info                   display OpenGL renderer info\n");
   printf("  -geometry WxH+X+Y       window geometry\n");
}
 

int
main(int argc, char *argv[])
{
   unsigned int winWidth = 300, winHeight = 300;
   int x = 0, y = 0;
   Display *dpy;
   Window win;
   GLXContext ctx;
   char *dpyName = NULL;
   GLboolean printInfo = GL_FALSE;
   VisualID visId;
   int i;

   dpy = XOpenDisplay(dpyName);
   if (!dpy) {
      printf("Error: couldn't open display %s\n",
	     dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
   }

   make_window(dpy, "glxgears", x, y, winWidth, winHeight, &win, &ctx, &visId);
   XMapWindow(dpy, win);
   glXMakeCurrent(dpy, win, ctx);
   query_vsync(dpy, win);

   printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
   printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
   printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));

   init();

   /* Set initial projection/viewing transformation.
    * We can't be sure we'll get a ConfigureNotify event when the window
    * first appears.
    */
   reshape(winWidth, winHeight);

   event_loop(dpy, win);

   glXMakeCurrent(dpy, None, NULL);
   glXDestroyContext(dpy, ctx);
   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);

   return 0;
}
