/*
 * Copyright (C) 2009  VMware, Inc.
 * Copyright (C) 1999-2006  Brian Paul
 * All Rights Reserved.
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
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/*
 * This program is a work-alike of the GLX glxinfo program.
 * Command line options:
 *  -t                     print wide table
 *  -v                     print verbose information
 *  -b                     only print ID of "best" visual on screen 0
 *  -l                     print interesting OpenGL limits (added 5 Sep 2002)
 */

#include <windows.h>

#include <GL/glew.h>
#include <GL/wglew.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "glinfo_common.h"


static GLboolean have_WGL_ARB_pixel_format;
static GLboolean have_WGL_ARB_multisample;

static PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB_func;


/**
 * An extension of PIXELFORMATDESCRIPTOR to handle multisample, etc.
 */
struct format_info {
   PIXELFORMATDESCRIPTOR pfd;
   int sampleBuffers, numSamples;
   int transparency;
};


static LRESULT CALLBACK
WndProc(HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam )
{
   switch (uMsg) {
   case WM_DESTROY:
      PostQuitMessage(0);
      break;
   default:
      return DefWindowProc(hWnd, uMsg, wParam, lParam);
   }

   return 0;
}


static void
print_screen_info(HDC _hdc, GLboolean limits, GLboolean singleLine)
{
   WNDCLASS wc;
   HWND win;
   HGLRC ctx;
   int visinfo;
   HDC hdc;
   PIXELFORMATDESCRIPTOR pfd;
   int version;
   const char *oglString = "OpenGL";

   memset(&wc, 0, sizeof wc);
   wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
   wc.hCursor = LoadCursor(NULL, IDC_ARROW);
   wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
   wc.lpfnWndProc = WndProc;
   wc.lpszClassName = "wglinfo";
   wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
   RegisterClass(&wc);

   win = CreateWindowEx(0,
                        wc.lpszClassName,
                        "wglinfo",
                        WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        NULL,
                        NULL,
                        wc.hInstance,
                        NULL);
   if (!win) {
      fprintf(stderr, "Couldn't create window\n");
      return;
   }

   hdc = GetDC(win);
   if (!hdc) {
      fprintf(stderr, "Couldn't obtain HDC\n");
      return;
   }

   pfd.cColorBits = 3;
   pfd.cRedBits = 1;
   pfd.cGreenBits = 1;
   pfd.cBlueBits = 1;
   pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
   pfd.iLayerType = PFD_MAIN_PLANE;
   pfd.iPixelType = PFD_TYPE_RGBA;
   pfd.nSize = sizeof(pfd);
   pfd.nVersion = 1;

   visinfo = ChoosePixelFormat(hdc, &pfd);
   if (!visinfo) {
      pfd.dwFlags |= PFD_DOUBLEBUFFER;
      visinfo = ChoosePixelFormat(hdc, &pfd);
   }

   if (!visinfo) {
      fprintf(stderr, "Error: couldn't find RGB WGL visual\n");
      return;
   }

   SetPixelFormat(hdc, visinfo, &pfd);
   ctx = wglCreateContext(hdc);
   if (!ctx) {
      fprintf(stderr, "Error: wglCreateContext failed\n");
      return;
   }

   if (wglMakeCurrent(hdc, ctx)) {
#if defined(WGL_ARB_extensions_string)
      PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB_func = 
         (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
#endif
      const char *glVendor = (const char *) glGetString(GL_VENDOR);
      const char *glRenderer = (const char *) glGetString(GL_RENDERER);
      const char *glVersion = (const char *) glGetString(GL_VERSION);
      const char *glExtensions = (const char *) glGetString(GL_EXTENSIONS);
      struct ext_functions extfuncs;
      
#if defined(WGL_ARB_extensions_string)
      if (wglGetExtensionsStringARB_func) {
         const char *wglExtensions = wglGetExtensionsStringARB_func(hdc);
         if(wglExtensions) {
            printf("WGL extensions:\n");
            print_extension_list(wglExtensions, singleLine);
         }
         if (extension_supported("WGL_ARB_pixel_format", wglExtensions)) {
            have_WGL_ARB_pixel_format = GL_TRUE;
         }
         if (extension_supported("WGL_ARB_multisample", wglExtensions)) {
            have_WGL_ARB_multisample = GL_TRUE;
         }
      }
#endif
      printf("OpenGL vendor string: %s\n", glVendor);
      printf("OpenGL renderer string: %s\n", glRenderer);
      printf("OpenGL version string: %s\n", glVersion);
#ifdef GL_VERSION_2_0
      if (glVersion[0] >= '2' && glVersion[1] == '.') {
         char *v = (char *) glGetString(GL_SHADING_LANGUAGE_VERSION);
         printf("OpenGL shading language version string: %s\n", v);
      }
#endif

      extfuncs.GetProgramivARB = (PFNGLGETPROGRAMIVARBPROC)
         wglGetProcAddress("glGetProgramivARB");
      extfuncs.GetStringi = (PFNGLGETSTRINGIPROC)
         wglGetProcAddress("glGetStringi");
      extfuncs.GetConvolutionParameteriv = (GETCONVOLUTIONPARAMETERIVPROC)
         wglGetProcAddress("glGetConvolutionParameteriv");

      version = (glVersion[0] - '0') * 10 + (glVersion[2] - '0');

      printf("OpenGL extensions:\n");
      print_extension_list(glExtensions, singleLine);
      if (limits) {
         print_limits(glExtensions, oglString, version, &extfuncs);
      }
   }
   else {
      fprintf(stderr, "Error: wglMakeCurrent failed\n");
   }

   DestroyWindow(win);
}


static const char *
visual_render_type_name(BYTE iPixelType)
{
   switch (iPixelType) {
      case PFD_TYPE_RGBA:
         return "rgba";
      case PFD_TYPE_COLORINDEX:
         return "ci";
      default:
         return "";
   }
}

static void
print_visual_attribs_verbose(int iPixelFormat, const struct format_info *info)
{
   printf("Visual ID: %x  generic=%d  native=%d\n",
          iPixelFormat, 
          info->pfd.dwFlags & PFD_GENERIC_FORMAT ? 1 : 0,
          info->pfd.dwFlags & PFD_DRAW_TO_WINDOW ? 1 : 0);
   printf("    bufferSize=%d level=%d renderType=%s doubleBuffer=%d stereo=%d\n",
          0 /* info->pfd.bufferSize */, 0 /* info->pfd.level */,
	  visual_render_type_name(info->pfd.iPixelType),
          info->pfd.dwFlags & PFD_DOUBLEBUFFER ? 1 : 0, 
          info->pfd.dwFlags & PFD_STEREO ? 1 : 0);
   printf("    rgba: cRedBits=%d cGreenBits=%d cBlueBits=%d cAlphaBits=%d\n",
          info->pfd.cRedBits, info->pfd.cGreenBits,
          info->pfd.cBlueBits, info->pfd.cAlphaBits);
   printf("    cAuxBuffers=%d cDepthBits=%d cStencilBits=%d\n",
          info->pfd.cAuxBuffers, info->pfd.cDepthBits, info->pfd.cStencilBits);
   printf("    accum: cRedBits=%d cGreenBits=%d cBlueBits=%d cAlphaBits=%d\n",
          info->pfd.cAccumRedBits, info->pfd.cAccumGreenBits,
          info->pfd.cAccumBlueBits, info->pfd.cAccumAlphaBits);
   printf("    multiSample=%d  multiSampleBuffers=%d\n",
          info->numSamples, info->sampleBuffers);
}


static void
print_visual_attribs_short_header(void)
{
 printf("   visual   x  bf lv rg d st colorbuffer ax dp st accumbuffer  ms  cav\n");
 printf(" id gen win sp sz l  ci b ro  r  g  b  a bf th cl  r  g  b  a ns b eat\n");
 printf("-----------------------------------------------------------------------\n");
}


static void
print_visual_attribs_short(int iPixelFormat, const struct format_info *info)
{
   char *caveat = "None";

   printf("0x%02x %2d  %2d %2d %2d %2d %c%c %c  %c %2d %2d %2d %2d %2d %2d %2d",
          iPixelFormat,
          info->pfd.dwFlags & PFD_GENERIC_FORMAT ? 1 : 0,
          info->pfd.dwFlags & PFD_DRAW_TO_WINDOW ? 1 : 0,
          info->transparency,
          info->pfd.cColorBits,
          0 /* info->pfd.level */,
          info->pfd.iPixelType == PFD_TYPE_RGBA ? 'r' : ' ',
          info->pfd.iPixelType == PFD_TYPE_COLORINDEX ? 'c' : ' ',
          info->pfd.dwFlags & PFD_DOUBLEBUFFER ? 'y' : '.',
          info->pfd.dwFlags & PFD_STEREO ? 'y' : '.',
          info->pfd.cRedBits, info->pfd.cGreenBits,
          info->pfd.cBlueBits, info->pfd.cAlphaBits,
          info->pfd.cAuxBuffers,
          info->pfd.cDepthBits,
          info->pfd.cStencilBits
          );

   printf(" %2d %2d %2d %2d %2d %1d %s\n",
          info->pfd.cAccumRedBits, info->pfd.cAccumGreenBits,
          info->pfd.cAccumBlueBits, info->pfd.cAccumAlphaBits,
          info->numSamples, info->sampleBuffers,
          caveat
          );
}


static void
print_visual_attribs_long_header(void)
{
 printf("Vis  Vis   Visual Trans  buff lev render DB ste  r   g   b   a  aux dep ste  accum buffers  MS   MS\n");
 printf(" ID Depth   Type  parent size el   type     reo sz  sz  sz  sz  buf th  ncl  r   g   b   a  num bufs\n");
 printf("----------------------------------------------------------------------------------------------------\n");
}


static void
print_visual_attribs_long(int iPixelFormat, const struct format_info *info)
{
   printf("0x%2x %2d %11d %2d     %2d %2d  %4s %3d %3d %3d %3d %3d %3d",
          iPixelFormat,
          info->pfd.dwFlags & PFD_GENERIC_FORMAT ? 1 : 0,
          info->pfd.dwFlags & PFD_DRAW_TO_WINDOW ? 1 : 0,
          0,
          0 /* info->pfd.bufferSize */,
          0 /* info->pfd.level */,
          visual_render_type_name(info->pfd.iPixelType),
          info->pfd.dwFlags & PFD_DOUBLEBUFFER ? 1 : 0,
          info->pfd.dwFlags & PFD_STEREO ? 1 : 0,
          info->pfd.cRedBits, info->pfd.cGreenBits,
          info->pfd.cBlueBits, info->pfd.cAlphaBits
          );

   printf(" %3d %4d %2d %3d %3d %3d %3d  %2d  %2d\n",
          info->pfd.cAuxBuffers,
          info->pfd.cDepthBits,
          info->pfd.cStencilBits,
          info->pfd.cAccumRedBits, info->pfd.cAccumGreenBits,
          info->pfd.cAccumBlueBits, info->pfd.cAccumAlphaBits,
          info->sampleBuffers, info->numSamples
          );
}


/**
 * Wrapper for wglGetPixelFormatAttribivARB()
 * \param attrib  the WGL_* attribute to query
 * \return value of the attribute, or 0 if failure
 */
static int
get_pf_attrib(HDC hdc, int pf, int attrib)
{
   int layer = 0, value;
   assert(have_WGL_ARB_pixel_format);
   if (wglGetPixelFormatAttribivARB_func(hdc, pf, layer, 1, &attrib, &value)) {
      return value;
   }
   else {
      return 0;
   }
}


/**
 * Fill in the format_info fields for the pixel format given by pf.
 */
static GLboolean
get_format_info(HDC hdc, int pf, struct format_info *info)
{
   memset(info, 0, sizeof(*info));

   if (have_WGL_ARB_pixel_format) {
      int swapMethod;

      info->pfd.dwFlags = 0;
      if (get_pf_attrib(hdc, pf, WGL_DRAW_TO_WINDOW_ARB))
         info->pfd.dwFlags |= PFD_DRAW_TO_WINDOW;
      if (!get_pf_attrib(hdc, pf, WGL_ACCELERATION_ARB))
         info->pfd.dwFlags |= PFD_GENERIC_FORMAT;
      if (get_pf_attrib(hdc, pf, WGL_SUPPORT_OPENGL_ARB))
         info->pfd.dwFlags |= PFD_SUPPORT_OPENGL;
      if (get_pf_attrib(hdc, pf, WGL_DOUBLE_BUFFER_ARB))
         info->pfd.dwFlags |= PFD_DOUBLEBUFFER;
      if (get_pf_attrib(hdc, pf, WGL_STEREO_ARB))
         info->pfd.dwFlags |= PFD_STEREO;

      swapMethod = get_pf_attrib(hdc, pf, WGL_SWAP_METHOD_ARB);
      if (swapMethod == WGL_SWAP_EXCHANGE_ARB)
         info->pfd.dwFlags |= PFD_SWAP_EXCHANGE;
      else if (swapMethod == WGL_SWAP_COPY_ARB)
         info->pfd.dwFlags |= PFD_SWAP_COPY;

      info->pfd.iPixelType = get_pf_attrib(hdc, pf, WGL_PIXEL_TYPE_ARB);
      if (info->pfd.iPixelType == WGL_TYPE_RGBA_ARB)
         info->pfd.iPixelType = PFD_TYPE_RGBA;
      else if (info->pfd.iPixelType == WGL_TYPE_COLORINDEX_ARB)
         info->pfd.iPixelType = PFD_TYPE_COLORINDEX;

      info->pfd.cColorBits = get_pf_attrib(hdc, pf, WGL_COLOR_BITS_ARB);
      info->pfd.cRedBits = get_pf_attrib(hdc, pf, WGL_RED_BITS_ARB);
      info->pfd.cGreenBits = get_pf_attrib(hdc, pf, WGL_GREEN_BITS_ARB);
      info->pfd.cBlueBits = get_pf_attrib(hdc, pf, WGL_BLUE_BITS_ARB);
      info->pfd.cAlphaBits = get_pf_attrib(hdc, pf, WGL_ALPHA_BITS_ARB);

      info->pfd.cDepthBits = get_pf_attrib(hdc, pf, WGL_DEPTH_BITS_ARB);
      info->pfd.cStencilBits = get_pf_attrib(hdc, pf, WGL_STENCIL_BITS_ARB);
      info->pfd.cAuxBuffers = get_pf_attrib(hdc, pf, WGL_AUX_BUFFERS_ARB);

      info->pfd.cAccumRedBits = get_pf_attrib(hdc, pf,
                                              WGL_ACCUM_RED_BITS_ARB);
      info->pfd.cAccumGreenBits = get_pf_attrib(hdc, pf,
                                                WGL_ACCUM_GREEN_BITS_ARB);
      info->pfd.cAccumBlueBits = get_pf_attrib(hdc, pf,
                                               WGL_ACCUM_BLUE_BITS_ARB);
      info->pfd.cAccumAlphaBits = get_pf_attrib(hdc, pf,
                                                WGL_ACCUM_ALPHA_BITS_ARB);

      info->sampleBuffers = get_pf_attrib(hdc, pf, WGL_SAMPLE_BUFFERS_ARB);
      info->numSamples = get_pf_attrib(hdc, pf, WGL_SAMPLES_ARB);

      info->transparency = get_pf_attrib(hdc, pf, WGL_TRANSPARENT_ARB);
   }
   else {
      if (!DescribePixelFormat(hdc, pf,
                               sizeof(PIXELFORMATDESCRIPTOR), &info->pfd))
         return GL_FALSE;
   }
   return GL_TRUE;
}



static void
print_visual_info(HDC hdc, InfoMode mode)
{
   struct format_info info;
   int numVisuals, numWglVisuals;
   int i;

   wglGetPixelFormatAttribivARB_func =
      (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
      wglGetProcAddress("wglGetPixelFormatAttribivARB");

   /* Get number of visuals / pixel formats */
   numVisuals = DescribePixelFormat(hdc, 1,
                                    sizeof(PIXELFORMATDESCRIPTOR), NULL);
   printf("%d Regular pixel formats\n", numVisuals);

   if (have_WGL_ARB_pixel_format) {
      int numExtVisuals = get_pf_attrib(hdc, 0, WGL_NUMBER_PIXEL_FORMATS_ARB);
      printf("%d Regular + Extended pixel formats\n", numExtVisuals);
      numVisuals = numExtVisuals;
   }

   if (numVisuals == 0)
      return;

   numWglVisuals = 0;
   for (i = 0; i < numVisuals; i++) {
      if(!DescribePixelFormat(hdc, i, sizeof(PIXELFORMATDESCRIPTOR), &info.pfd))
	 continue;

      //if(!(info.pfd.dwFlags & PFD_SUPPORT_OPENGL))
      //   continue;

      ++numWglVisuals;
   }

   printf("%d WGL Visuals\n", numWglVisuals);

   if (mode == Normal)
      print_visual_attribs_short_header();
   else if (mode == Wide)
      print_visual_attribs_long_header();

   for (i = 0; i < numVisuals; i++) {
      get_format_info(hdc, i, &info);

      if (mode == Verbose)
	 print_visual_attribs_verbose(i, &info);
      else if (mode == Normal)
         print_visual_attribs_short(i, &info);
      else if (mode == Wide) 
         print_visual_attribs_long(i, &info);
   }
   printf("\n");
}


/*
 * Examine all visuals to find the so-called best one.
 * We prefer deepest RGBA buffer with depth, stencil and accum
 * that has no caveats.
 */
static int
find_best_visual(HDC hdc)
{
#if 0
   XVisualInfo theTemplate;
   XVisualInfo *visuals;
   int numVisuals;
   long mask;
   int i;
   struct visual_attribs bestVis;

   /* get list of all visuals on this screen */
   theTemplate.screen = scrnum;
   mask = VisualScreenMask;
   visuals = XGetVisualInfo(hdc, mask, &theTemplate, &numVisuals);

   /* init bestVis with first visual info */
   get_visual_attribs(hdc, &visuals[0], &bestVis);

   /* try to find a "better" visual */
   for (i = 1; i < numVisuals; i++) {
      struct visual_attribs vis;

      get_visual_attribs(hdc, &visuals[i], &vis);

      /* always skip visuals with caveats */
      if (vis.visualCaveat != GLX_NONE_EXT)
         continue;

      /* see if this vis is better than bestVis */
      if ((!bestVis.supportsGL && vis.supportsGL) ||
          (bestVis.visualCaveat != GLX_NONE_EXT) ||
          (bestVis.iPixelType != vis.iPixelType) ||
          (!bestVis.doubleBuffer && vis.doubleBuffer) ||
          (bestVis.cRedBits < vis.cRedBits) ||
          (bestVis.cGreenBits < vis.cGreenBits) ||
          (bestVis.cBlueBits < vis.cBlueBits) ||
          (bestVis.cAlphaBits < vis.cAlphaBits) ||
          (bestVis.cDepthBits < vis.cDepthBits) ||
          (bestVis.cStencilBits < vis.cStencilBits) ||
          (bestVis.cAccumRedBits < vis.cAccumRedBits)) {
         /* found a better visual */
         bestVis = vis;
      }
   }

   return bestVis.id;
#else
   return 0;
#endif
}



int
main(int argc, char *argv[])
{
   HDC hdc;
   struct options opts;

   parse_args(argc, argv, &opts);

   hdc = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);

   if (opts.findBest) {
      int b;
      b = find_best_visual(hdc);
      printf("%d\n", b);
   }
   else {
      print_screen_info(hdc, opts.limits, opts.singleLine);
      printf("\n");
      print_visual_info(hdc, opts.mode);
   }

   return 0;
}
