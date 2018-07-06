/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Rob Bradford <rob@linux.intel.com>
 *   Kristian Høgsberg (from eglkms.c)
 *   Benjamin Franzke (from eglkms.c)
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <glib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "cogl-winsys-egl-kms-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-kms-renderer.h"
#include "cogl-kms-display.h"
#include "cogl-version.h"
#include "cogl-error-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererKMS
{
  int fd;
  struct gbm_device *gbm;
  CoglPollFD poll_fd;
} CoglRendererKMS;

typedef struct _CoglOutputKMS
{
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  drmModeCrtc *saved_crtc;
  drmModeModeInfo *modes;
  int n_modes;
  drmModeModeInfo mode;
} CoglOutputKMS;

typedef struct _CoglDisplayKMS
{
  GList *outputs;
  int width, height;
  CoglBool pending_set_crtc;
  CoglBool pending_swap_notify;
  struct gbm_surface *dummy_gbm_surface;
} CoglDisplayKMS;

typedef struct _CoglFlipKMS
{
  CoglOnscreen *onscreen;
  int pending;
} CoglFlipKMS;

typedef struct _CoglOnscreenKMS
{
  struct gbm_surface *surface;
  uint32_t current_fb_id;
  uint32_t next_fb_id;
  struct gbm_bo *current_bo;
  struct gbm_bo *next_bo;
  CoglBool pending_swap_notify;
} CoglOnscreenKMS;

static const char device_name[] = "/dev/dri/card0";

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  eglTerminate (egl_renderer->edpy);

  g_slice_free (CoglRendererKMS, kms_renderer);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererKMS *kms_renderer;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;
  egl_renderer->platform = g_slice_new0 (CoglRendererKMS);
  kms_renderer = egl_renderer->platform;

  kms_renderer->fd = open (device_name, O_RDWR);
  if (kms_renderer->fd < 0)
    {
      /* Probably permissions error */
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't open %s", device_name);
      return FALSE;
    }

  kms_renderer->gbm = gbm_create_device (kms_renderer->fd);
  if (kms_renderer->gbm == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't create gbm device");
      goto close_fd;
    }

  egl_renderer->edpy = eglGetDisplay ((EGLNativeDisplayType)kms_renderer->gbm);
  if (egl_renderer->edpy == EGL_NO_DISPLAY)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Couldn't get eglDisplay");
      goto destroy_gbm_device;
    }

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto egl_terminate;

  kms_renderer->poll_fd.fd = kms_renderer->fd;
  kms_renderer->poll_fd.events = COGL_POLL_FD_EVENT_IN;

  return TRUE;

egl_terminate:
  eglTerminate (egl_renderer->edpy);
destroy_gbm_device:
  gbm_device_destroy (kms_renderer->gbm);
close_fd:
  close (kms_renderer->fd);

  _cogl_winsys_renderer_disconnect (renderer);

  return FALSE;
}

static CoglBool
is_connector_excluded (int id,
                       int *excluded_connectors,
                       int n_excluded_connectors)
{
  int i;
  for (i = 0; i < n_excluded_connectors; i++)
    if (excluded_connectors[i] == id)
      return TRUE;
  return FALSE;
}

static drmModeConnector *
find_connector (int fd,
                drmModeRes *resources,
                int *excluded_connectors,
                int n_excluded_connectors)
{
  int i;

  for (i = 0; i < resources->count_connectors; i++)
    {
      drmModeConnector *connector =
        drmModeGetConnector (fd, resources->connectors[i]);

      if (connector &&
          connector->connection == DRM_MODE_CONNECTED &&
          connector->count_modes > 0 &&
          !is_connector_excluded (connector->connector_id,
                                  excluded_connectors,
                                  n_excluded_connectors))
        return connector;
      drmModeFreeConnector(connector);
    }
  return NULL;
}

static CoglBool
find_mirror_modes (drmModeModeInfo *modes0,
                   int n_modes0,
                   drmModeModeInfo *modes1,
                   int n_modes1,
                   drmModeModeInfo *mode1_out,
                   drmModeModeInfo *mode0_out)
{
  int i;
  for (i = 0; i < n_modes0; i++)
    {
      int j;
      drmModeModeInfo *mode0 = &modes0[i];
      for (j = 0; j < n_modes1; j++)
        {
          drmModeModeInfo *mode1 = &modes1[j];
          if (mode1->hdisplay == mode0->hdisplay &&
              mode1->vdisplay == mode0->vdisplay)
            {
              *mode0_out = *mode0;
              *mode1_out = *mode1;
              return TRUE;
            }
        }
    }
  return FALSE;
}

static drmModeModeInfo builtin_1024x768 =
{
	63500,			/* clock */
	1024, 1072, 1176, 1328, 0,
	768, 771, 775, 798, 0,
	59920,
	DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	0,
	"1024x768"
};

static CoglBool
is_panel (int type)
{
  return (type == DRM_MODE_CONNECTOR_LVDS ||
          type == DRM_MODE_CONNECTOR_eDP);
}

static CoglOutputKMS *
find_output (int _index,
             int fd,
             drmModeRes *resources,
             int *excluded_connectors,
             int n_excluded_connectors,
             CoglError **error)
{
  char *connector_env_name = g_strdup_printf ("COGL_KMS_CONNECTOR%d", _index);
  char *mode_env_name;
  drmModeConnector *connector;
  drmModeEncoder *encoder;
  CoglOutputKMS *output;
  drmModeModeInfo *modes;
  int n_modes;

  if (getenv (connector_env_name))
    {
      unsigned long id = strtoul (getenv (connector_env_name), NULL, 10);
      connector = drmModeGetConnector (fd, id);
    }
  else
    connector = NULL;
  g_free (connector_env_name);

  if (connector == NULL)
    connector = find_connector (fd, resources,
                                excluded_connectors, n_excluded_connectors);
  if (connector == NULL)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "No currently active connector found");
      return NULL;
    }

  /* XXX: At this point it seems connector->encoder_id may be an invalid id of 0
   * even though the connector is marked as connected. Referencing ->encoders[0]
   * seems more reliable. */
  encoder = drmModeGetEncoder (fd, connector->encoders[0]);

  output = g_slice_new0 (CoglOutputKMS);
  output->connector = connector;
  output->encoder = encoder;
  output->saved_crtc = drmModeGetCrtc (fd, encoder->crtc_id);

  if (is_panel (connector->connector_type))
    {
      n_modes = connector->count_modes + 1;
      modes = g_new (drmModeModeInfo, n_modes);
      memcpy (modes, connector->modes,
              sizeof (drmModeModeInfo) * connector->count_modes);
      /* TODO: parse EDID */
      modes[n_modes - 1] = builtin_1024x768;
    }
  else
    {
      n_modes = connector->count_modes;
      modes = g_new (drmModeModeInfo, n_modes);
      memcpy (modes, connector->modes,
              sizeof (drmModeModeInfo) * n_modes);
    }

  mode_env_name = g_strdup_printf ("COGL_KMS_CONNECTOR%d_MODE", _index);
  if (getenv (mode_env_name))
    {
      const char *name = getenv (mode_env_name);
      int i;
      CoglBool found = FALSE;
      drmModeModeInfo mode;

      for (i = 0; i < n_modes; i++)
        {
          if (strcmp (modes[i].name, name) == 0)
            {
              found = TRUE;
              break;
            }
        }
      if (!found)
        {
          g_free (mode_env_name);
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "COGL_KMS_CONNECTOR%d_MODE of %s could not be found",
                       _index, name);
          return NULL;
        }
      n_modes = 1;
      mode = modes[i];
      g_free (modes);
      modes = g_new (drmModeModeInfo, 1);
      modes[0] = mode;
    }
  g_free (mode_env_name);

  output->modes = modes;
  output->n_modes = n_modes;

  return output;
}

static void
setup_crtc_modes (CoglDisplay *display, int fb_id)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;

  for (l = kms_display->outputs; l; l = l->next)
    {
      CoglOutputKMS *output = l->data;
      int ret = drmModeSetCrtc (kms_renderer->fd,
                                output->encoder->crtc_id,
                                fb_id, 0, 0,
                                &output->connector->connector_id, 1,
                                &output->mode);
      if (ret)
        g_warning ("Failed to set crtc mode %s: %m", output->mode.name);
    }
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display;
  CoglRendererEGL *egl_renderer = display->renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  drmModeRes *resources;
  CoglOutputKMS *output0, *output1;
  CoglBool mirror;

  kms_display = g_slice_new0 (CoglDisplayKMS);
  egl_display->platform = kms_display;

  resources = drmModeGetResources (kms_renderer->fd);
  if (!resources)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "drmModeGetResources failed");
      return FALSE;
    }

  output0 = find_output (0,
                         kms_renderer->fd,
                         resources,
                         NULL,
                         0, /* n excluded connectors */
                         error);
  kms_display->outputs = g_list_append (kms_display->outputs, output0);
  if (!output0)
    return FALSE;

  if (getenv ("COGL_KMS_MIRROR"))
    mirror = TRUE;
  else
    mirror = FALSE;

  if (mirror)
    {
      int exclude_connector = output0->connector->connector_id;
      output1 = find_output (1,
                             kms_renderer->fd,
                             resources,
                             &exclude_connector,
                             1, /* n excluded connectors */
                             error);
      if (!output1)
        return FALSE;

      kms_display->outputs = g_list_append (kms_display->outputs, output1);

      if (!find_mirror_modes (output0->modes, output0->n_modes,
                              output1->modes, output1->n_modes,
                              &output0->mode,
                              &output1->mode))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_INIT,
                       "Failed to find matching modes for mirroring");
          return FALSE;
        }
    }
  else
    output0->mode = output0->modes[0];

  kms_display->width = output0->mode.hdisplay;
  kms_display->height = output0->mode.vdisplay;

  /* We defer setting the crtc modes until the first swap_buffers request of a
   * CoglOnscreen framebuffer. */
  kms_display->pending_set_crtc = TRUE;

  return TRUE;
}

static void
output_free (int fd, CoglOutputKMS *output)
{
  if (output->modes)
    g_free (output->modes);

  if (output->encoder)
    drmModeFreeEncoder (output->encoder);

  if (output->connector)
    {
      if (output->saved_crtc)
        {
          int ret = drmModeSetCrtc (fd,
                                    output->saved_crtc->crtc_id,
                                    output->saved_crtc->buffer_id,
                                    output->saved_crtc->x,
                                    output->saved_crtc->y,
                                    &output->connector->connector_id, 1,
                                    &output->saved_crtc->mode);
          if (ret)
            g_warning (G_STRLOC ": Error restoring saved CRTC");
        }
      drmModeFreeConnector (output->connector);
    }

  g_slice_free (CoglOutputKMS, output);
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  GList *l;

  for (l = kms_display->outputs; l; l = l->next)
    output_free (kms_renderer->fd, l->data);
  g_list_free (kms_display->outputs);
  kms_display->outputs = NULL;

  g_slice_free (CoglDisplayKMS, egl_display->platform);
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  kms_display->dummy_gbm_surface = gbm_surface_create (kms_renderer->gbm,
                                                       16, 16,
                                                       GBM_FORMAT_XRGB8888,
                                                       GBM_BO_USE_RENDERING);
  if (!kms_display->dummy_gbm_surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to create dummy GBM surface");
      return FALSE;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) kms_display->dummy_gbm_surface,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to create dummy EGL surface");
      return FALSE;
    }

  if (!_cogl_winsys_egl_make_current (display,
                                      egl_display->dummy_surface,
                                      egl_display->dummy_surface,
                                      egl_display->egl_context))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Failed to make context current");
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (kms_display->dummy_gbm_surface != NULL)
    {
      gbm_surface_destroy (kms_display->dummy_gbm_surface);
      kms_display->dummy_gbm_surface = NULL;
    }
}

static void
free_current_bo (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  if (kms_onscreen->current_fb_id)
    {
      drmModeRmFB (kms_renderer->fd,
                   kms_onscreen->current_fb_id);
      kms_onscreen->current_fb_id = 0;
    }
  if (kms_onscreen->current_bo)
    {
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->current_bo);
      kms_onscreen->current_bo = NULL;
    }
}

static void
page_flip_handler (int fd,
                   unsigned int frame,
                   unsigned int sec,
                   unsigned int usec,
                   void *data)
{
  CoglFlipKMS *flip = data;
  CoglOnscreen *onscreen = flip->onscreen;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;

  /* We're only ready to dispatch a swap notification once all outputs
   * have flipped... */
  flip->pending--;
  if (flip->pending == 0)
    {
      /* We only want to notify that the swap is complete when the application
       * calls cogl_context_dispatch so instead of immediately notifying we'll
       * set a flag to remember to notify later */
      kms_display->pending_swap_notify = TRUE;
      kms_onscreen->pending_swap_notify = TRUE;

      free_current_bo (onscreen);

      kms_onscreen->current_fb_id = kms_onscreen->next_fb_id;
      kms_onscreen->next_fb_id = 0;

      kms_onscreen->current_bo = kms_onscreen->next_bo;
      kms_onscreen->next_bo = NULL;

      cogl_object_unref (flip->onscreen);

      g_slice_free (CoglFlipKMS, flip);
    }
}

static void
handle_drm_event (CoglRendererKMS *kms_renderer)
{
  drmEventContext evctx;

  memset (&evctx, 0, sizeof evctx);
  evctx.version = DRM_EVENT_CONTEXT_VERSION;
  evctx.page_flip_handler = page_flip_handler;
  drmHandleEvent (kms_renderer->fd, &evctx);
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplayEGL *egl_display = context->display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;
  uint32_t handle, stride;
  CoglFlipKMS *flip;
  GList *l;

  /* If we already have a pending swap then block until it completes */
  while (kms_onscreen->next_fb_id != 0)
    handle_drm_event (kms_renderer);

  /* First chain-up. This will call eglSwapBuffers */
  parent_vtable->onscreen_swap_buffers (onscreen);

  /* Now we need to set the CRTC to whatever is the front buffer */
  kms_onscreen->next_bo = gbm_surface_lock_front_buffer (kms_onscreen->surface);

#if (COGL_VERSION_ENCODE (COGL_GBM_MAJOR, COGL_GBM_MINOR, COGL_GBM_MICRO) >= \
     COGL_VERSION_ENCODE (8, 1, 0))
  stride = gbm_bo_get_stride (kms_onscreen->next_bo);
#else
  stride = gbm_bo_get_pitch (kms_onscreen->next_bo);
#endif
  handle = gbm_bo_get_handle (kms_onscreen->next_bo).u32;

  if (drmModeAddFB (kms_renderer->fd,
                    kms_display->width,
                    kms_display->height,
                    24, /* depth */
                    32, /* bpp */
                    stride,
                    handle,
                    &kms_onscreen->next_fb_id))
    {
      g_warning ("Failed to create new back buffer handle: %m");
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->next_bo);
      kms_onscreen->next_bo = NULL;
      kms_onscreen->next_fb_id = 0;
      return;
    }

  /* If this is the first framebuffer to be presented then we now setup the
   * crtc modes... */
  if (kms_display->pending_set_crtc)
    {
      setup_crtc_modes (context->display, kms_onscreen->next_fb_id);
      kms_display->pending_set_crtc = FALSE;
    }

  flip = g_slice_new0 (CoglFlipKMS);
  flip->onscreen = onscreen;

  for (l = kms_display->outputs; l; l = l->next)
    {
      CoglOutputKMS *output = l->data;

      if (drmModePageFlip (kms_renderer->fd,
                           output->encoder->crtc_id,
                           kms_onscreen->next_fb_id,
                           DRM_MODE_PAGE_FLIP_EVENT,
                           flip))
        {
          g_warning ("Failed to flip: %m");
          continue;
        }

      flip->pending++;
    }

  if (flip->pending == 0)
    {
      drmModeRmFB (kms_renderer->fd, kms_onscreen->next_fb_id);
      gbm_surface_release_buffer (kms_onscreen->surface,
                                  kms_onscreen->next_bo);
      kms_onscreen->next_bo = NULL;
      kms_onscreen->next_fb_id = 0;
      g_slice_free (CoglFlipKMS, flip);
      flip = NULL;
    }
  else
    {
      /* Ensure the onscreen remains valid while it has any pending flips... */
      cogl_object_ref (flip->onscreen);
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_SWAP_BUFFERS_EVENT, TRUE);
  /* TODO: remove this deprecated feature */
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT,
                  TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                  TRUE);

  return TRUE;
}

static CoglBool
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenKMS *kms_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (egl_display->egl_context, FALSE);

  onscreen->winsys = g_slice_new0 (CoglOnscreenEGL);
  egl_onscreen = onscreen->winsys;

  kms_onscreen = g_slice_new0 (CoglOnscreenKMS);
  egl_onscreen->platform = kms_onscreen;

  kms_onscreen->surface =
    gbm_surface_create (kms_renderer->gbm,
                        kms_display->width,
                        kms_display->height,
                        GBM_BO_FORMAT_XRGB8888,
                        GBM_BO_USE_SCANOUT |
                        GBM_BO_USE_RENDERING);

  if (!kms_onscreen->surface)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (NativeWindowType) kms_onscreen->surface,
                            NULL);
  if (egl_onscreen->egl_surface == EGL_NO_SURFACE)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Failed to allocate surface");
      return FALSE;
    }

  _cogl_framebuffer_winsys_update_size (framebuffer,
                                        kms_display->width,
                                        kms_display->height);

  return TRUE;
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenKMS *kms_onscreen;

  /* If we never successfully allocated then there's nothing to do */
  if (egl_onscreen == NULL)
    return;

  kms_onscreen = egl_onscreen->platform;

  /* flip state takes a reference on the onscreen so there should
   * never be outstanding flips when we reach here. */
  g_return_if_fail (kms_onscreen->next_fb_id == 0);

  free_current_bo (onscreen);

  if (egl_onscreen->egl_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_onscreen->egl_surface);
      egl_onscreen->egl_surface = EGL_NO_SURFACE;
    }

  if (kms_onscreen->surface)
    {
      gbm_surface_destroy (kms_onscreen->surface);
      kms_onscreen->surface = NULL;
    }

  g_slice_free (CoglOnscreenKMS, kms_onscreen);
  g_slice_free (CoglOnscreenEGL, onscreen->winsys);
  onscreen->winsys = NULL;
}

static void
_cogl_winsys_poll_get_info (CoglContext *context,
                            CoglPollFD **poll_fds,
                            int *n_poll_fds,
                            int64_t *timeout)
{
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;

  *poll_fds = &kms_renderer->poll_fd;
  *n_poll_fds = 1;

  /* If we've already got a pending swap notify then we'll dispatch
     immediately */
  *timeout = kms_display->pending_swap_notify ? 0 : -1;
}

static void
flush_pending_swap_notify_cb (void *data,
                              void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenKMS *kms_onscreen = egl_onscreen->platform;

      if (kms_onscreen->pending_swap_notify)
        {
          CoglFrameInfo *info = g_queue_pop_head (&onscreen->pending_frame_infos);

          _cogl_onscreen_notify_frame_sync (onscreen, info);
          _cogl_onscreen_notify_complete (onscreen, info);
          kms_onscreen->pending_swap_notify = FALSE;

          cogl_object_unref (info);
        }
    }
}

static void
_cogl_winsys_poll_dispatch (CoglContext *context,
                            const CoglPollFD *poll_fds,
                            int n_poll_fds)
{
  CoglDisplay *display = context->display;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayKMS *kms_display = egl_display->platform;
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererKMS *kms_renderer = egl_renderer->platform;
  int i;

  for (i = 0; i < n_poll_fds; i++)
    if (poll_fds[i].fd == kms_renderer->fd)
      {
        if (poll_fds[i].revents)
          handle_drm_event (kms_renderer);

        break;
      }

  if (kms_display->pending_swap_notify)
    {
      g_list_foreach (context->framebuffers,
                      flush_pending_swap_notify_cb,
                      NULL);
      kms_display->pending_swap_notify = FALSE;
    }
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init
  };

const CoglWinsysVtable *
_cogl_winsys_egl_kms_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_KMS winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_KMS;
      vtable.name = "EGL_KMS";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;

      /* The KMS winsys doesn't support swap region */
      vtable.onscreen_swap_region = NULL;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;

      vtable.poll_get_info = _cogl_winsys_poll_get_info;
      vtable.poll_dispatch = _cogl_winsys_poll_dispatch;

      vtable_inited = TRUE;
    }

  return &vtable;
}

int
cogl_kms_renderer_get_kms_fd (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), -1);

  if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererKMS *kms_renderer = egl_renderer->platform;
      return kms_renderer->fd;
    }
  else
    return -1;
}

void
cogl_kms_display_queue_modes_reset (CoglDisplay *display)
{
  if (display->setup)
    {
      CoglDisplayEGL *egl_display = display->winsys;
      CoglDisplayKMS *kms_display = egl_display->platform;
      kms_display->pending_set_crtc = TRUE;
    }
}
