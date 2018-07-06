/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2011 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*
 * This file is derived from the "native-activity" sample of the android NDK
 * r5b. The coding style has been adapted to the code style most commonly found
 * in glib/gobject based projects.
 */

#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <glib.h>
#include <glib-android/glib-android.h>

/**
 * Shared state for our app
 */
typedef struct
{
  struct android_app* app;

  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
} TestData;

/**
 * Initialize an EGL context for the current display
 */
static int test_init_display (TestData *data)
{
  /*
   * Here specify the attributes of the desired configuration
   * Below, we select an EGLConfig with at least 8 bits per color
   * component compatible with on-screen windows
   */
  const EGLint attribs[] =
  {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_NONE
  };
  EGLint format;
  EGLint numConfigs;
  EGLConfig config;
  EGLSurface surface;
  EGLContext context;

  EGLDisplay display = eglGetDisplay (EGL_DEFAULT_DISPLAY);

  eglInitialize (display, 0, 0);

  /* Here, the application chooses the configuration it desires. In this
   * sample, we have a very simplified selection process, where we pick
   * the first EGLConfig that matches our criteria */
  eglChooseConfig (display, attribs, &config, 1, &numConfigs);

  /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
   * guaranteed to be accepted by ANativeWindow_setBuffersGeometry ().
   * As soon as we picked a EGLConfig, we can safely reconfigure the
   * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
  eglGetConfigAttrib (display, config, EGL_NATIVE_VISUAL_ID, &format);

  ANativeWindow_setBuffersGeometry (data->app->window, 0, 0, format);

  surface = eglCreateWindowSurface (display, config, data->app->window, NULL);
  context = eglCreateContext (display, config, NULL, NULL);

  if (eglMakeCurrent (display, surface, surface, context) == EGL_FALSE)
    {
      g_warning ("Unable to eglMakeCurrent");
      return -1;
    }

  data->display = display;
  data->context = context;
  data->surface = surface;

  /* Initialize GL state */
  glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  glEnable (GL_CULL_FACE);
  glShadeModel (GL_SMOOTH);
  glDisable (GL_DEPTH_TEST);

  return 0;
}

/**
 * Just the current frame in the display
 */
static void test_draw_frame (TestData *data)
{
  if (data->display == NULL)
    return;

  glClearColor (1.f, 0.f, 1.f, 1.f);
  glClear (GL_COLOR_BUFFER_BIT);

  eglSwapBuffers (data->display, data->surface);
}

/**
 * Tear down the EGL context currently associated with the display
 */
static void test_term_display (TestData *data)
{
  if (data->display != EGL_NO_DISPLAY)
    {
      eglMakeCurrent (data->display,
                      EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      if (data->context != EGL_NO_CONTEXT)
        eglDestroyContext (data->display, data->context);

      if (data->surface != EGL_NO_SURFACE)
        eglDestroySurface (data->display, data->surface);

      eglTerminate (data->display);
    }
  data->display = EGL_NO_DISPLAY;
  data->context = EGL_NO_CONTEXT;
  data->surface = EGL_NO_SURFACE;
}

/**
 * Process the next input event
 */
static int32_t
test_handle_input (struct android_app* app,
                   AInputEvent*        event)
{
  if (AInputEvent_getType (event) == AINPUT_EVENT_TYPE_MOTION)
    {
      g_message ("motion event: (%.02lf,%0.2lf)",
                 AMotionEvent_getX (event, 0),
                 AMotionEvent_getY (event, 0));
      return 1;
    }

  return 0;
}

/**
 * Process the next main command
 */
static void
test_handle_cmd (struct android_app* app,
                 int32_t             cmd)
{
  TestData *data = (TestData *) app->userData;

  switch (cmd)
    {
    case APP_CMD_SAVE_STATE:
      g_message ("command: SAVE_STATE");
      break;

    case APP_CMD_INIT_WINDOW:
      g_message ("command: INIT_WINDOW");
      if (data->app->window != NULL)
        {
          test_init_display (data);
          test_draw_frame (data);
        }
      break;

    case APP_CMD_TERM_WINDOW:
      g_message ("command: TERM_WINDOW");
      test_term_display (data);
      break;

    case APP_CMD_GAINED_FOCUS:
      g_message ("command: GAINED_FOCUS");
      break;

    case APP_CMD_LOST_FOCUS:
      g_message ("command: LOST_FOCUS");
      test_draw_frame (data);
      break;
    }
}

static gboolean
print_message (gpointer data)
{
  gchar *message = data;

  g_message ("%s", message);

  return TRUE;
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things
 */
void
android_main (struct android_app* application)
{
  TestData data;
  GMainLoop *main_loop;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (TestData));
  application->userData = &data;
  application->onAppCmd = test_handle_cmd;
  application->onInputEvent = test_handle_input;
  data.app = application;

  main_loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_seconds (5, print_message, "Hello, World!");

  g_main_loop_run (main_loop);
}
