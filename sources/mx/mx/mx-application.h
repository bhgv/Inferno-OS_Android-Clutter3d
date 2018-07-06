/*
 * mx-application: application class
 *
 * Copyright 2010, 2012 Intel Corporation.
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
 * Written by: Thomas Wood <thomas.wood@intel.com>
 */

#if !defined(MX_H_INSIDE) && !defined(MX_COMPILATION)
#error "Only <mx/mx.h> can be included directly.h"
#endif

#ifndef _MX_APPLICATION_H
#define _MX_APPLICATION_H

#include <clutter/clutter.h>
#include <mx/mx-action.h>
#include <mx/mx-window.h>

G_BEGIN_DECLS

#define MX_TYPE_APPLICATION mx_application_get_type()

#define MX_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  MX_TYPE_APPLICATION, MxApplication))

#define MX_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  MX_TYPE_APPLICATION, MxApplicationClass))

#define MX_IS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  MX_TYPE_APPLICATION))

#define MX_IS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  MX_TYPE_APPLICATION))

#define MX_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  MX_TYPE_APPLICATION, MxApplicationClass))

typedef struct _MxApplication MxApplication;
typedef struct _MxApplicationClass MxApplicationClass;
typedef struct _MxApplicationPrivate MxApplicationPrivate;

/**
 * MxApplication:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _MxApplication
{
  GApplication parent;

  MxApplicationPrivate *priv;
};

struct _MxApplicationClass
{
  GApplicationClass parent_class;

  /* padding for future expansion */
  void (*_padding_0) (void);
  void (*_padding_1) (void);
  void (*_padding_2) (void);
  void (*_padding_3) (void);
  void (*_padding_4) (void);
};

GType mx_application_get_type (void) G_GNUC_CONST;


MxApplication* mx_application_new           (const gchar       *application_id,
                                             GApplicationFlags  flags);

void           mx_application_add_window    (MxApplication *application,
                                             MxWindow      *window);
void           mx_application_remove_window (MxApplication *application,
                                             MxWindow      *window);

const GList*   mx_application_get_windows   (MxApplication *application);

MxWindow*      mx_application_create_window (MxApplication *application,
                                             const gchar   *window_title);
G_END_DECLS

#endif /* _MX_APPLICATION_H */
