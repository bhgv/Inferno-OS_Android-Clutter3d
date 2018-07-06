/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * mx-viewport.h: Viewport actor
 *
 * Copyright 2008 OpenedHand
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 * Port to Mx by: Robert Staudinger <robsta@openedhand.com>
 *
 */

#if !defined(MX_H_INSIDE) && !defined(MX_COMPILATION)
#error "Only <mx/mx.h> can be included directly.h"
#endif

#ifndef __MX_VIEWPORT_H__
#define __MX_VIEWPORT_H__

#include <mx/mx-widget.h>

G_BEGIN_DECLS

#define MX_TYPE_VIEWPORT            (mx_viewport_get_type())
#define MX_VIEWPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MX_TYPE_VIEWPORT, MxViewport))
#define MX_IS_VIEWPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MX_TYPE_VIEWPORT))
#define MX_VIEWPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MX_TYPE_VIEWPORT, MxViewportClass))
#define MX_IS_VIEWPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MX_TYPE_VIEWPORT))
#define MX_VIEWPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MX_TYPE_VIEWPORT, MxViewportClass))

typedef struct _MxViewport          MxViewport;
typedef struct _MxViewportPrivate   MxViewportPrivate;
typedef struct _MxViewportClass     MxViewportClass;

/**
 * MxViewport:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _MxViewport
{
  /*< private >*/
  MxWidget parent;

  MxViewportPrivate *priv;
};

struct _MxViewportClass
{
  MxWidgetClass parent_class;

  /* padding for future expansion */
  void (*_padding_0) (void);
  void (*_padding_1) (void);
  void (*_padding_2) (void);
  void (*_padding_3) (void);
  void (*_padding_4) (void);
};

GType mx_viewport_get_type (void) G_GNUC_CONST;

ClutterActor *mx_viewport_new (void);

void mx_viewport_set_origin (MxViewport *viewport,
                             gfloat      x,
                             gfloat      y,
                             gfloat      z);
void mx_viewport_get_origin (MxViewport *viewport,
                             gfloat     *x,
                             gfloat     *y,
                             gfloat     *z);

void mx_viewport_set_sync_adjustments (MxViewport *viewport,
                                       gboolean    sync);
gboolean mx_viewport_get_sync_adjustments (MxViewport *viewport);

G_END_DECLS

#endif /* __MX_VIEWPORT_H__ */
