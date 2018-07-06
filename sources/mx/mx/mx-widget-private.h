/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * mx-widget.h: Base class for Mx actors
 *
 * Copyright 2007 OpenedHand
 * Copyright 2008, 2009 Intel Corporation.
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
 */

#ifndef __MX_WIDGET_PRIVATE_H__
#define __MX_WIDGET_PRIVATE_H__

#include <mx-widget.h>

G_BEGIN_DECLS

void     _mx_widget_add_touch_sequence    (MxWidget             *widget,
                                           ClutterEventSequence *sequence);
void     _mx_widget_remove_touch_sequence (MxWidget             *widget,
                                           ClutterEventSequence *sequence);
gboolean _mx_widget_has_touch_sequence    (MxWidget             *widget,
                                           ClutterEventSequence *sequence);
gboolean _mx_widget_has_touch_sequences   (MxWidget *widget);

G_END_DECLS

#endif /* __MX_WIDGET_PRIVATE_H__ */
