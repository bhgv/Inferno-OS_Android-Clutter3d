/*
 * mx-window-wayland: MxNativeWindow implementation for Wayland
 *
 * Copyright 2012 Intel Corporation.
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
 *
 * Written by: Rob Bradford <rob@linux.intel.com>
 */

#ifndef _MX_WINDOW_WAYLAND_H
#define _MX_WINDOW_WAYLAND_H

#include <glib-object.h>
#include "mx-window.h"
#include "mx-native-window.h"

G_BEGIN_DECLS

#define MX_TYPE_WINDOW_WAYLAND mx_window_wayland_get_type()

#define MX_WINDOW_WAYLAND(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  MX_TYPE_WINDOW_WAYLAND, MxWindowWayland))

#define MX_WINDOW_WAYLAND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  MX_TYPE_WINDOW_WAYLAND, MxWindowWaylandClass))

#define MX_IS_WINDOW_WAYLAND(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  MX_TYPE_WINDOW_WAYLAND))

#define MX_IS_WINDOW_WAYLAND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  MX_TYPE_WINDOW_WAYLAND))

#define MX_WINDOW_WAYLAND_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  MX_TYPE_WINDOW_WAYLAND, MxWindowWaylandClass))

typedef struct _MxWindowWayland MxWindowWayland;
typedef struct _MxWindowWaylandClass MxWindowWaylandClass;
typedef struct _MxWindowWaylandPrivate MxWindowWaylandPrivate;

struct _MxWindowWayland
{
  GObject parent;

  MxWindowWaylandPrivate *priv;
};

struct _MxWindowWaylandClass
{
  GObjectClass parent_class;
};

GType mx_window_wayland_get_type (void) G_GNUC_CONST;

MxNativeWindow *_mx_window_wayland_new (MxWindow *window);

G_END_DECLS

#endif /* _MX_WINDOW_WAYLAND_H */
