/*
 * Copyright (C) 2011 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
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
 * Authors:
 *  Damien Lespiau <damien.lespiau@gmail.com>
 */

#include <errno.h>
#include <string.h>

#include <android/log.h>
#include <android/looper.h>

#include <android_native_app_glue.h>

#include "glib-android.h"

#define G_ANDROID_DEBUG 0

#if G_ANDROID_DEBUG

#define G_ANDROID_NOTE(fmt,args...) g_debug (G_STRLOC ": " fmt, ##args);

static const gchar *
looper_id_to_string (int id)
{
  static gchar buffer[32];

  if (id == LOOPER_ID_MAIN)
    return "MAIN";

  if (id == LOOPER_ID_INPUT)
    return "INPUT";

  if (id == LOOPER_ID_USER)
    return "USER";

  if (id > LOOPER_ID_USER)
    {
      g_snprintf (buffer, 32, "USER + %d", id);
      return buffer;
    }

  return "Unknown id";
}
#else

#define G_ANDROID_NOTE(fmt,args...) G_STMT_END { } G_STMT_END

#endif

static android_LogPriority
g_log_to_android_log (GLogLevelFlags flags)
{
  guint i;
  static long levels[8] =
    {
      0,                      /* (unused) ANDROID_LOG_UNKNOWN */
      0,                      /* (unused) ANDROID_LOG_DEFAULT */
      G_LOG_LEVEL_MESSAGE,    /* ANDROID_LOG_VERBOSE */
      G_LOG_LEVEL_DEBUG,      /* ANDROID_LOG_DEBUG */
      G_LOG_LEVEL_INFO,       /* ANDROID_LOG_INFO */
      G_LOG_LEVEL_WARNING,    /* ANDROID_LOG_WARN */
      G_LOG_LEVEL_CRITICAL,   /* ANDROID_LOG_ERROR */
      G_LOG_LEVEL_ERROR       /* ANDROID_LOG_FATAL */
    };

  for (i = 3; i < G_N_ELEMENTS (levels); i++)
    if (flags & levels [i])
      break;

  if (i == G_N_ELEMENTS (levels))
    return ANDROID_LOG_INFO;

  return i;
}

static void
g_android_log_handler (const gchar    *log_domain,
                       GLogLevelFlags  log_level,
                       const gchar    *message,
                       gpointer        user_data)
{
  gboolean is_fatal = (log_level & G_LOG_FLAG_FATAL);
  android_LogPriority android_level;

  if (is_fatal)
    android_level = ANDROID_LOG_FATAL;
  else
    android_level = g_log_to_android_log (log_level);

  __android_log_write (android_level, log_domain, message);
}

static gint
g_io_condition_to_looper_event (GIOCondition condition)
{
  int flags = 0;

  if (condition & G_IO_IN)
    flags |= ALOOPER_EVENT_INPUT;

  if (condition & G_IO_OUT)
    flags |= ALOOPER_EVENT_OUTPUT;

  if (condition & G_IO_ERR)
    flags |= ALOOPER_EVENT_ERROR;

  if (condition & G_IO_HUP)
    flags |= ALOOPER_EVENT_HANGUP;

  if (condition & G_IO_NVAL)
    flags |= ALOOPER_EVENT_INVALID;

  return flags;
}

static GIOCondition
looper_event_to_g_io_condition (int events)
{
  int flags = 0;

  if (events & ALOOPER_EVENT_INPUT)
    flags |= G_IO_IN;

  if (events & ALOOPER_EVENT_OUTPUT)
    flags |= G_IO_OUT;

  if (events & ALOOPER_EVENT_ERROR)
    flags |= G_IO_ERR;

  if (events & ALOOPER_EVENT_HANGUP)
    flags |= G_IO_HUP;

  if (events & ALOOPER_EVENT_INVALID)
    flags |= G_IO_NVAL;

  return flags;
}
/*
 * It's totally fine to have a static variable to hold the state between two
 * g_android_poll() invocations as I only plan to use this function for GLib's
 * default context only. I assume that you want integration with ALooper for
 * events and sensors from the GLib's main thread, the other threads can then
 * just the the normal polling function.
 *
 * We need to save the fds between invocations of g_android_poll() to keep
 * track of which fd has been removed from the list to remove them from the
 * ALooper.
 */
static GPrivate tls_previous_fds = G_PRIVATE_INIT ((GDestroyNotify) g_array_unref);

static GArray *
_get_previous_fds (void)
{
  return g_private_get (&tls_previous_fds);
}

static void
_set_previous_fds (GArray *array)
{
  g_private_replace (&tls_previous_fds, array);
}

static void
save_poll_state (GPollFD *fds,
                 gint    n_fds)
{
  GArray *previous_fds = _get_previous_fds ();

  if (G_UNLIKELY (previous_fds == NULL))
    {
      previous_fds = g_array_sized_new (FALSE, FALSE, sizeof (GPollFD), n_fds);
      _set_previous_fds (previous_fds);
    }

  g_array_set_size (previous_fds, 0);
  g_array_insert_vals (previous_fds, 0, fds, n_fds);
}

static gboolean
has_fd (GPollFD *fds,
        guint    n_fds,
        gint     fd)
{
  guint i;

  for (i = 0; i < n_fds; i++)
    if (fds[i].fd == fd)
      return TRUE;

  return FALSE;
}

/*
 * Timer used when we need to re-enter the poll after handling LOOPER_ID_MAIN
 * and LOOPER_ID_INPUT events with an updated timeout value
 */
static GTimer *remaining_timeout;

/*
 * Note: Having to do some bookkeeping ourselves to add/remove fds involves
 * O(n^2) operations, not great for a large number of fd...
 *
 * XXX: handle ALOOPER_EVENT_WAKE
 */
static gint
g_android_poll (GPollFD *fds,
                guint    n_fds,
                gint     timeout_)
{
  ALooper *looper;
  gint res, out_fd, out_events;
  guint i;
  void *out_data;
  GArray *previous_fds;

  looper = ALooper_forThread ();
  if (G_UNLIKELY (looper == NULL))
    {
      g_critical ("Could not retrieve the ALooper object");
      return -1;
    }

  /* Re-adding a fd to the ALooper replaces it if previously added. It is safe
   * to re-add all the fds we are given */
  for (i = 0; i < n_fds; i ++)
    {
      int events;

      G_ANDROID_NOTE ("Add fd %d", fds[i].fd);

      events = g_io_condition_to_looper_event (fds[i].events);
      res = ALooper_addFd (looper, fds[i].fd, LOOPER_ID_USER + i, events,
                           NULL, NULL);
      if (G_UNLIKELY (res == -1))
        g_warning ("Could not add fd %d to looper", fds[i].fd);
    }

  /* Remove the fds that were in the previous call but are not present any
   * longer */
  previous_fds = _get_previous_fds ();

  if (G_LIKELY (previous_fds != NULL))
    {
      for (i = 0; i < previous_fds->len; i++)
        {
          GPollFD *fd = &g_array_index (previous_fds, GPollFD, i);

          if (!has_fd (fds, n_fds, fd->fd))
            {
              G_ANDROID_NOTE ("Remove fd %d", fds[i].fd);

              res = ALooper_removeFd (looper, fd->fd);

              if (G_UNLIKELY (res == 0))
                {
                  g_warning ("We tried to remove the fd %d but it wasn't in "
                             "the looper", fd->fd);
                }
              else if (G_UNLIKELY (res == -1))
                {
                  g_warning ("Could not remove fd %d from looper", fd->fd);
                }
            }
        }
    }

  /* It's time to poll now */
poll:
  G_ANDROID_NOTE ("Waiting in pollAll() for %dms", timeout_);
  g_timer_start (remaining_timeout);
  res = ALooper_pollAll (timeout_, &out_fd, &out_events, &out_data);

  /* save the fds we've been called with, we can return from the function
   * pretty soon now */
  save_poll_state (fds, n_fds);

  /* FIXME: let's assume that the underlying function behind ALooper_pollAll()
   * sets errno */
  if (res == ALOOPER_POLL_ERROR)
    {
      G_ANDROID_NOTE ("pollAll() returned an error (%d:%s)", errno,
                      strerror (errno));
      return -1;
    }

  /* A value of 0 indicates that the call timed out and no file descriptors
   * were ready */
  if (res == ALOOPER_POLL_TIMEOUT)
    {
      G_ANDROID_NOTE ("pollAll() timed out (%dms)", timeout_);
      return 0;
    }

  G_ANDROID_NOTE ("Processing id %s", looper_id_to_string (res));

  /* The glue provided in the NDK installs those two ids in the looper */
  if (res == LOOPER_ID_MAIN || res == LOOPER_ID_INPUT)
    {
      struct android_poll_source *source = out_data;
      gdouble elapsed;
      gint elapsed_ms;

      if (source && source->process)
        source->process (source->app, source);

      if (timeout_ < 0)
        goto poll;

      /* compute the new timeout, note this is done after processing the
       * MAIN and INPUT source, so we effectively take into account the time
       * we just spent in those process() functions */
      elapsed = g_timer_elapsed (remaining_timeout, NULL);
      elapsed_ms = elapsed * 1000;

      timeout_ -= elapsed_ms;
      if (timeout_ < 0)
        return 0;

      goto poll;
    }

  /* We've been signaled a fd, let's update GPollFD.revents. res is the ident
   * we've given in addFd(), we can extract the index in fds from it */
  i = res - LOOPER_ID_USER;
  G_ANDROID_NOTE ("Signalling fd %d", fds[i].fd);
  fds[i].revents = looper_event_to_g_io_condition (out_events);
  ALooper_removeFd (looper, fds[i].fd);

  return 1;
}

gboolean
g_android_init (void)
{
  GMainContext *context;

  /* logs */
  g_log_set_default_handler (g_android_log_handler, NULL);

  /* main loop */
  remaining_timeout = g_timer_new ();

  context = g_main_context_default ();
  g_main_context_set_poll_func (context, g_android_poll);

  return TRUE;
}
