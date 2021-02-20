/*
Looking Glass - KVM FrameRelay (KVMFR) Client
Copyright (C) 2021 Guanzhong Chen (quantum2048@gmail.com)
Copyright (C) 2021 Tudor Brindus (contact@tbrindus.ca)
https://looking-glass.io

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "wayland.h"

#include <stdbool.h>
#include <string.h>

#include <EGL/egl.h>
#include <wayland-client.h>

#include "app.h"
#include "common/debug.h"

#if defined(ENABLE_EGL) || defined(ENABLE_OPENGL)
#include "egl_dynprocs.h"

bool waylandEGLInit(int w, int h)
{
  wlWm.eglWindow = wl_egl_window_create(wlWm.surface, w, h);
  if (!wlWm.eglWindow)
  {
    DEBUG_ERROR("Failed to create EGL window");
    return false;
  }
  return true;
}

EGLDisplay waylandGetEGLDisplay(void)
{
  EGLNativeDisplayType native = (EGLNativeDisplayType) wlWm.display;

  const char *early_exts = eglQueryString(NULL, EGL_EXTENSIONS);

  if (strstr(early_exts, "EGL_KHR_platform_wayland") != NULL &&
      g_egl_dynProcs.eglGetPlatformDisplay)
  {
    DEBUG_INFO("Using eglGetPlatformDisplay");
    return g_egl_dynProcs.eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, native, NULL);
  }

  if (strstr(early_exts, "EGL_EXT_platform_wayland") != NULL &&
      g_egl_dynProcs.eglGetPlatformDisplayEXT)
  {
    DEBUG_INFO("Using eglGetPlatformDisplayEXT");
    return g_egl_dynProcs.eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, native, NULL);
  }

  DEBUG_INFO("Using eglGetDisplay");
  return eglGetDisplay(native);
}

void waylandEGLSwapBuffers(EGLDisplay display, EGLSurface surface)
{
  eglSwapBuffers(display, surface);

  if (wlWm.resizeSerial)
  {
    wl_egl_window_resize(wlWm.eglWindow, wlWm.width, wlWm.height, 0, 0);

    struct wl_region * region = wl_compositor_create_region(wlWm.compositor);
    wl_region_add(region, 0, 0, wlWm.width, wlWm.height);
    wl_surface_set_opaque_region(wlWm.surface, region);
    wl_region_destroy(region);

    app_handleResizeEvent(wlWm.width, wlWm.height, (struct Border) {0, 0, 0, 0});
    xdg_surface_ack_configure(wlWm.xdgSurface, wlWm.resizeSerial);
    wlWm.resizeSerial = 0;
  }
}
#endif

#ifdef ENABLE_EGL
EGLNativeWindowType waylandGetEGLNativeWindow(void)
{
  return (EGLNativeWindowType) wlWm.eglWindow;
}
#endif

#ifdef ENABLE_OPENGL
bool waylandOpenGLInit(void)
{
  EGLint attr[] =
  {
    EGL_BUFFER_SIZE      , 24,
    EGL_CONFORMANT       , EGL_OPENGL_BIT,
    EGL_RENDERABLE_TYPE  , EGL_OPENGL_BIT,
    EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
    EGL_RED_SIZE         , 8,
    EGL_GREEN_SIZE       , 8,
    EGL_BLUE_SIZE        , 8,
    EGL_SAMPLE_BUFFERS   , 0,
    EGL_SAMPLES          , 0,
    EGL_NONE
  };

  wlWm.glDisplay = waylandGetEGLDisplay();

  int maj, min;
  if (!eglInitialize(wlWm.glDisplay, &maj, &min))
  {
    DEBUG_ERROR("Unable to initialize EGL");
    return false;
  }

  if (wlWm.glDisplay == EGL_NO_DISPLAY)
  {
    DEBUG_ERROR("Failed to get EGL display (eglError: 0x%x)", eglGetError());
    return false;
  }

  EGLint num_config;
  if (!eglChooseConfig(wlWm.glDisplay, attr, &wlWm.glConfig, 1, &num_config))
  {
    DEBUG_ERROR("Failed to choose config (eglError: 0x%x)", eglGetError());
    return false;
  }

  wlWm.glSurface = eglCreateWindowSurface(wlWm.glDisplay, wlWm.glConfig, wlWm.eglWindow, NULL);
  if (wlWm.glSurface == EGL_NO_SURFACE)
  {
    DEBUG_ERROR("Failed to create EGL surface (eglError: 0x%x)", eglGetError());
    return false;
  }

  return true;
}

LG_DSGLContext waylandGLCreateContext(void)
{
  eglBindAPI(EGL_OPENGL_API);
  return eglCreateContext(wlWm.glDisplay, wlWm.glConfig, EGL_NO_CONTEXT, NULL);
}

void waylandGLDeleteContext(LG_DSGLContext context)
{
  eglDestroyContext(wlWm.glDisplay, context);
}

void waylandGLMakeCurrent(LG_DSGLContext context)
{
  eglMakeCurrent(wlWm.glDisplay, wlWm.glSurface, wlWm.glSurface, context);
}

void waylandGLSetSwapInterval(int interval)
{
  eglSwapInterval(wlWm.glDisplay, interval);
}

void waylandGLSwapBuffers(void)
{
  waylandEGLSwapBuffers(wlWm.glDisplay, wlWm.glSurface);
}
#endif