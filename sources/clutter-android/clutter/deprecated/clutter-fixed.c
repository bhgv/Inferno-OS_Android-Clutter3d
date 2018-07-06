/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#define G_IMPLEMENT_INLINES

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS

/* This file depends on the cogl-fixed api which isn't exposed when
 * COGL_ENABLE_EXPERIMENTAL_2_0_API is defined...
 */
#undef COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl.h>

#include <glib-object.h>

#include "clutter-fixed.h"
#include "clutter-private.h"

/**
 * clutter_value_set_fixed: (skip)
 * @value: a #GValue initialized to %COGL_TYPE_FIXED
 * @fixed_: the fixed point value to set
 *
 * Sets @value to @fixed_.
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Use g_value_set_int() instead.
 */
void
clutter_value_set_fixed (GValue    *value,
                         CoglFixed  fixed_)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value));

  value->data[0].v_int = fixed_;
}

/**
 * clutter_value_get_fixed: (skip)
 * @value: a #GValue initialized to %COGL_TYPE_FIXED
 *
 * Gets the fixed point value stored inside @value.
 *
 * Return value: the value inside the passed #GValue
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Use g_value_get_int() instead.
 */
CoglFixed
clutter_value_get_fixed (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value), 0);

  return value->data[0].v_int;
}

static void
param_fixed_init (GParamSpec *pspec)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);

  fspec->minimum = COGL_FIXED_MIN;
  fspec->maximum = COGL_FIXED_MAX;
  fspec->default_value = 0;
}

static void
param_fixed_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  value->data[0].v_int = CLUTTER_PARAM_SPEC_FIXED (pspec)->default_value;
}

static gboolean
param_fixed_validate (GParamSpec *pspec,
                      GValue     *value)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);
  gint oval = value->data[0].v_int;
  gint min, max, val;

  g_assert (CLUTTER_IS_PARAM_SPEC_FIXED (pspec));

  /* we compare the integer part of the value because the minimum
   * and maximum values cover just that part of the representation
   */
  min = fspec->minimum;
  max = fspec->maximum;
  val =  (value->data[0].v_int);

  val = CLAMP (val, min, max);
  if (val != oval)
    {
      value->data[0].v_int = val;
      return TRUE;
    }

  return FALSE;
}

static gint
param_fixed_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

GType
clutter_param_fixed_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecFixed),
        16,
        param_fixed_init,
        COGL_TYPE_FIXED,
        NULL,
        param_fixed_set_default,
        param_fixed_validate,
        param_fixed_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecFixed"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_fixed: (skip)
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #CoglFixed values
 *
 * Return value: (transfer full): the newly created #GParamSpec
 *
 * Since: 0.8
 *
 * Deprecated: 1.10: Use #GParamSpecInt instead.
 */
GParamSpec *
clutter_param_spec_fixed (const gchar *name,
                          const gchar *nick,
                          const gchar *blurb,
                          CoglFixed    minimum,
                          CoglFixed    maximum,
                          CoglFixed    default_value,
                          GParamFlags  flags)
{
  ClutterParamSpecFixed *fspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum,
                        NULL);

  fspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_FIXED,
                                 name, nick, blurb,
                                 flags);
  fspec->minimum = minimum;
  fspec->maximum = maximum;
  fspec->default_value = default_value;

  return G_PARAM_SPEC (fspec);
}
