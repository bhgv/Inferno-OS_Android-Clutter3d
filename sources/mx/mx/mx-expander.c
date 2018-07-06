/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * mx-expander.c: Expander Widget
 *
 * Copyright 2009, 2010, 2012 Intel Corporation.
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
 *
 */

/**
 * SECTION:mx-expander
 * @short_description: a container which the user can show or hide its child
 *
 * #MxExpander is a single child container that allows the user to show or
 * hide its child. It displays a clickable bar (with a text label),
 * which (by default) when clicked toggles display of the child.
 *
 * <figure id="mx-expander-expanded">
 *   <title>MxExpander in its expanded state</title>
 *   <para>The image shows an #MxExpander with the
 *   #MxExpander:expanded property set to
 *   #TRUE.</para>
 *   <graphic fileref="MxExpander-expanded.png" format="PNG"/>
 * </figure>
 *
 * <figure id="mx-expander-contracted">
 *   <title>MxExpander in its contracted state</title>
 *   <para>The image shows an #MxExpander with the
 *   #MxExpander:expanded property set to
 *   #FALSE.</para>
 *   <graphic fileref="MxExpander-contracted.png" format="PNG"/>
 * </figure>
 */

#include "mx-marshal.h"
#include "mx-expander.h"
#include "mx-private.h"
#include "mx-stylable.h"
#include "mx-icon.h"

G_DEFINE_TYPE (MxExpander, mx_expander, MX_TYPE_WIDGET)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_EXPANDER, MxExpanderPrivate))

enum {
  PROP_0,

  PROP_EXPANDED,
  PROP_LABEL
};

enum
{
  EXPAND_COMPLETE,

  LAST_SIGNAL
};

struct _MxExpanderPrivate {
  ClutterActor    *label;
  ClutterActor    *arrow;
  gfloat           spacing;

  ClutterTimeline *timeline;
  gdouble          progress;

  guint            expanded : 1;

  ClutterActor    *child;
};

static guint expander_signals[LAST_SIGNAL] = { 0, };

static void
mx_expander_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MxExpanderPrivate *priv = MX_EXPANDER (object)->priv;

  switch (property_id) {
    case PROP_EXPANDED:
      g_value_set_boolean (value, priv->expanded);
      break;
    case PROP_LABEL:
      g_value_set_string (value, clutter_text_get_text (CLUTTER_TEXT (priv->label)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_expander_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (property_id) {
    case PROP_EXPANDED:
      mx_expander_set_expanded ((MxExpander *) object,
                                g_value_get_boolean (value));
      break;
    case PROP_LABEL:
      mx_expander_set_label ((MxExpander *) object,
                             g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_expander_dispose (GObject *object)
{
  MxExpanderPrivate *priv = MX_EXPANDER (object)->priv;

  if (priv->label)
    {
      clutter_actor_remove_child (CLUTTER_ACTOR (object), priv->label);
      priv->label = NULL;
    }

  if (priv->arrow)
    {
      clutter_actor_remove_child (CLUTTER_ACTOR (object), priv->arrow);
      priv->arrow = NULL;
    }

  if (priv->timeline)
    {
      g_object_unref (priv->timeline);
      priv->timeline = NULL;
    }

  G_OBJECT_CLASS (mx_expander_parent_class)->dispose (object);
}

static void
mx_expander_finalize (GObject *object)
{
  G_OBJECT_CLASS (mx_expander_parent_class)->finalize (object);
}

static void
timeline_complete (ClutterTimeline *timeline,
                   ClutterActor    *expander)
{
  guchar opacity;
  MxExpanderPrivate *priv = MX_EXPANDER (expander)->priv;

  g_signal_emit (expander, expander_signals[EXPAND_COMPLETE], 0);

  /* if the expander is now closed, update the style */
  if (!priv->expanded)
    {
      clutter_actor_set_name (priv->arrow, "mx-expander-arrow-closed");
      mx_stylable_set_style_class (MX_STYLABLE (expander), "closed-expander");

      clutter_actor_queue_relayout (expander);
    }

  if (!priv->child)
    return;

  /* continue only if we are "opening" */
  if (!priv->expanded)
    return;

  /* we can't do an animation if there is already one in progress,
   * because we cannot get the actors original opacity */
  if (clutter_actor_get_transition (priv->child, "opacity"))
    {
      clutter_actor_show (priv->child);
      return;
    }

  opacity = clutter_actor_get_opacity (priv->child);
  clutter_actor_set_opacity (priv->child, 0);

  clutter_actor_show (priv->child);

  clutter_actor_save_easing_state (priv->child);
  clutter_actor_set_easing_mode (priv->child, CLUTTER_EASE_IN_SINE);
  clutter_actor_set_easing_duration (priv->child, 100);
  clutter_actor_set_opacity (priv->child, opacity);
  clutter_actor_restore_easing_state (priv->child);
}

static void
new_frame (ClutterTimeline *timeline,
           gint             frame_num,
           ClutterActor    *expander)
{
  MxExpanderPrivate *priv = MX_EXPANDER (expander)->priv;

  priv->progress = clutter_timeline_get_progress (priv->timeline);

  clutter_actor_queue_relayout (expander);
}

static void
mx_expander_update (MxExpander *expander)
{
  MxExpanderPrivate *priv = expander->priv;

  if (priv->expanded)
    {
      clutter_actor_set_name (priv->arrow, "mx-expander-arrow-open");
      mx_stylable_set_style_class (MX_STYLABLE (expander), "open-expander");
    }
  /* closed state is set when animation is finished */

  if (!priv->child)
    return;

  /* setup and start the expansion animation */
  if (!priv->expanded)
    {
      clutter_actor_hide (priv->child);
      clutter_timeline_set_direction (priv->timeline,
                                      CLUTTER_TIMELINE_BACKWARD);
    }
  else
    {
      clutter_timeline_set_direction (priv->timeline,
                                      CLUTTER_TIMELINE_FORWARD);
    }


  if (!clutter_timeline_is_playing (priv->timeline))
    clutter_timeline_rewind (priv->timeline);

  clutter_timeline_start (priv->timeline);
}

static gboolean
mx_expander_button_press (ClutterActor       *actor,
                          ClutterButtonEvent *event)
{
  return TRUE;
}

static gboolean
mx_expander_button_release (ClutterActor       *actor,
                            ClutterButtonEvent *event)
{
  MxExpanderPrivate *priv = MX_EXPANDER (actor)->priv;

  mx_expander_set_expanded (MX_EXPANDER (actor), !priv->expanded);

  return TRUE;
}

static void
mx_expander_get_preferred_width (ClutterActor *actor,
                                 gfloat        for_height,
                                 gfloat       *min_width,
                                 gfloat       *pref_width)
{
  MxExpanderPrivate *priv = MX_EXPANDER (actor)->priv;
  MxPadding padding;
  gfloat min_child_w, pref_child_w, min_label_w, pref_label_w, arrow_w;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  if (priv->child)
    {
      clutter_actor_get_preferred_width (priv->child,
                                         -1,
                                         &min_child_w,
                                         &pref_child_w);
    }
  else
    {
      min_child_w = 0;
      pref_child_w = 0;
    }

  clutter_actor_get_preferred_width (priv->label,
                                     -1, &min_label_w, &pref_label_w);
  clutter_actor_get_preferred_width (priv->arrow,
                                     -1, NULL, &arrow_w);

  /* TODO: create a style property for this padding between arrow and label */
  if (arrow_w)
    arrow_w += 6.0f;

  if (min_width)
    *min_width = padding.left
                 + MAX (min_child_w, min_label_w + arrow_w)
                 + padding.right;

  if (pref_width)
    *pref_width = padding.left
                  + MAX (pref_child_w, pref_label_w + arrow_w)
                  + padding.right;
}

static void
mx_expander_get_preferred_height (ClutterActor *actor,
                                  gfloat        for_width,
                                  gfloat       *min_height,
                                  gfloat       *pref_height)
{
  MxExpanderPrivate *priv = MX_EXPANDER (actor)->priv;
  MxPadding padding;
  gfloat min_child_h, pref_child_h, min_label_h, pref_label_h, arrow_h;
  gfloat available_w;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  available_w = for_width - padding.left - padding.right;

  if (priv->child)
    {
      clutter_actor_get_preferred_height (priv->child,
                                          available_w,
                                          &min_child_h,
                                          &pref_child_h);
      min_child_h += priv->spacing;
      pref_child_h += priv->spacing;

      /* allocate the space multiplied by the progress of the "expansion"
       * animation */
      min_child_h *= priv->progress;
      pref_child_h *= priv->progress;
    }
  else
    {
      min_child_h = 0;
      pref_child_h = 0;
    }

  clutter_actor_get_preferred_height (priv->label,
                                      available_w,
                                      &min_label_h,
                                      &pref_label_h);

  clutter_actor_get_preferred_height (priv->arrow, -1, NULL, &arrow_h);

  min_label_h = MAX (min_label_h, arrow_h);
  pref_label_h = MAX (pref_label_h, arrow_h);

  if (min_height)
    *min_height = padding.top
                  + min_child_h + min_label_h
                  + padding.bottom;

  if (pref_height)
    *pref_height = padding.top
                   + pref_child_h + pref_label_h
                   + padding.bottom;

}

static void
mx_expander_allocate (ClutterActor          *actor,
                      const ClutterActorBox *box,
                      ClutterAllocationFlags flags)
{
  MxExpanderPrivate *priv = MX_EXPANDER (actor)->priv;
  ClutterActorBox child_box;
  MxPadding padding;
  gfloat label_w, label_h;
  gfloat available_w, available_h, min_w, min_h, arrow_h, arrow_w;

  /* chain up to store allocation */
  CLUTTER_ACTOR_CLASS (mx_expander_parent_class)->allocate (actor, box, flags);

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  available_w = (box->x2 - box->x1) - padding.left - padding.right;
  available_h = (box->y2 - box->y1) - padding.top - padding.bottom;

  /* arrow */
  clutter_actor_get_preferred_width (priv->arrow, -1, NULL, &arrow_w);
  arrow_w = MIN (arrow_w, available_w);

  clutter_actor_get_preferred_height (priv->arrow, -1, NULL, &arrow_h);
  arrow_h = MIN (arrow_h, available_h);

  child_box.x1 = padding.left;
  child_box.x2 = child_box.x1 + arrow_w;
  child_box.y1 = padding.top;
  child_box.y2 = child_box.y1 + arrow_h;
  clutter_actor_allocate (priv->arrow, &child_box, flags);

  /* label */
  min_h = 0;
  min_w = 0;

  clutter_actor_get_preferred_width (priv->label,
                                     available_h, &min_w, &label_w);
  label_w = CLAMP (label_w, min_w, available_w);

  clutter_actor_get_preferred_height (priv->label,
                                      label_w, &min_h, &label_h);
  label_h = CLAMP (label_h, min_h, available_h);

  /* TODO: make a style property for padding between arrow and label */
  child_box.x1 = padding.left + arrow_w + 6.0f;
  child_box.x2 = child_box.x1 + label_w;
  child_box.y1 = padding.top;
  child_box.y2 = child_box.y1 + MAX (label_h, arrow_h);
  mx_allocate_align_fill (priv->label, &child_box, MX_ALIGN_START,
                          MX_ALIGN_MIDDLE, FALSE, FALSE);
  clutter_actor_allocate (priv->label, &child_box, flags);

  /* remove label height and spacing for child calculations */
  available_h -= MAX (label_h, arrow_h) + priv->spacing;

  /* child */
  if (priv->expanded && priv->child && CLUTTER_ACTOR_IS_VISIBLE (priv->child))
    {
      child_box.x1 = padding.left;
      child_box.x2 = child_box.x1 + available_w;
      child_box.y1 = padding.top + priv->spacing + MAX (label_h, arrow_h);
      child_box.y2 = child_box.y1 + available_h;

      clutter_actor_allocate (priv->child, &child_box, flags);
    }
}

static void
mx_expander_paint (ClutterActor *actor)
{
  MxExpanderPrivate *priv = ((MxExpander* ) actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_expander_parent_class)->paint (actor);

  clutter_actor_paint (priv->label);
  clutter_actor_paint (priv->arrow);

  if (priv->expanded)
    clutter_actor_paint (priv->child);
}

static void
mx_expander_pick (ClutterActor       *actor,
                  const ClutterColor *color)
{
  MxExpanderPrivate *priv = ((MxExpander* ) actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_expander_parent_class)->pick (actor, color);

  if (priv->expanded)
    clutter_actor_paint (priv->child);
}

static void
mx_expander_style_changed (MxStylable *stylable)
{
  MxExpander *expander = MX_EXPANDER (stylable);
  MxExpanderPrivate *priv = expander->priv;
  const gchar *pseudo_class;

  pseudo_class = mx_stylable_get_style_pseudo_class (stylable);

  mx_stylable_set_style_pseudo_class (MX_STYLABLE (expander->priv->arrow),
                                      pseudo_class);

  mx_stylable_apply_clutter_text_attributes (stylable,
                                             CLUTTER_TEXT (priv->label));
}

static void
mx_expander_actor_added (ClutterActor *container,
                         ClutterActor *child)
{
  MxExpanderPrivate *priv = MX_EXPANDER (container)->priv;

  if (MX_IS_TOOLTIP (child))
    return;

  if (priv->child)
    clutter_actor_remove_child (container, priv->child);

  priv->child = child;

  if (!priv->expanded)
    clutter_actor_hide (child);
}

static void
mx_expander_actor_removed (ClutterActor *container,
                           ClutterActor *child)
{
  MxExpanderPrivate *priv = MX_EXPANDER (container)->priv;

  if (priv->child == child)
    priv->child = NULL;
}

static void
mx_expander_class_init (MxExpanderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MxExpanderPrivate));

  object_class->get_property = mx_expander_get_property;
  object_class->set_property = mx_expander_set_property;
  object_class->dispose = mx_expander_dispose;
  object_class->finalize = mx_expander_finalize;

  actor_class->button_press_event = mx_expander_button_press;
  actor_class->button_release_event = mx_expander_button_release;
  actor_class->allocate = mx_expander_allocate;
  actor_class->get_preferred_width = mx_expander_get_preferred_width;
  actor_class->get_preferred_height = mx_expander_get_preferred_height;
  actor_class->paint = mx_expander_paint;
  actor_class->pick = mx_expander_pick;

  pspec = g_param_spec_boolean ("expanded",
                                "Expanded",
                                "Indicates that the expander is open or closed",
                                FALSE,
                                MX_PARAM_READWRITE);
  g_object_class_install_property (object_class,
                                   PROP_EXPANDED,
                                   pspec);

  pspec = g_param_spec_string ("label",
                               "Label",
                               "Expander title label.",
                               NULL,
                               G_PARAM_READWRITE | MX_PARAM_TRANSLATEABLE);
  g_object_class_install_property (object_class,
                                   PROP_LABEL,
                                   pspec);

  /**
   * MxExpander::expand-complete:
   * @expander: the object that received the signal
   *
   * Emitted after the expand animation finishes. Check the "expanded" property
   * of the #MxExpander to determine if the expander is expanded or not.
   */

  expander_signals[EXPAND_COMPLETE] =
    g_signal_new ("expand-complete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MxExpanderClass, expand_complete),
                  NULL, NULL,
                  _mx_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
mx_expander_init (MxExpander *self)
{
  MxExpanderPrivate *priv = self->priv = GET_PRIVATE (self);

  priv->label = clutter_text_new ();
  clutter_actor_add_child ((ClutterActor *) self, priv->label);

  priv->arrow = (ClutterActor *) mx_icon_new ();
  clutter_actor_add_child ((ClutterActor*) self, priv->arrow);
  clutter_actor_set_name (priv->arrow, "mx-expander-arrow-closed");

  /* TODO: make this a style property */
  priv->spacing = 10.0f;

  priv->timeline = clutter_timeline_new (250);
  clutter_timeline_set_progress_mode (priv->timeline, CLUTTER_EASE_IN_SINE);
  g_signal_connect (priv->timeline, "new-frame", G_CALLBACK (new_frame), self);
  g_signal_connect (priv->timeline, "completed", G_CALLBACK (timeline_complete), self);


  clutter_actor_set_reactive ((ClutterActor *) self, TRUE);

  g_signal_connect (self, "style-changed",
                    G_CALLBACK (mx_expander_style_changed), NULL);

  g_signal_connect (self, "actor-added",
                    G_CALLBACK (mx_expander_actor_added), NULL);
  g_signal_connect (self, "actor-removed",
                    G_CALLBACK (mx_expander_actor_removed), NULL);
}

/**
 * mx_expander_new:
 *
 * Creates a new #MxExpander
 *
 * Returns: the newly allocated #MxExpander
 */
ClutterActor *
mx_expander_new (void)
{
  return g_object_new (MX_TYPE_EXPANDER, NULL);
}

/**
 * mx_expander_set_label:
 * @expander: A #MxExpander
 * @label: string to set as the expander label
 *
 * Sets the text displayed as the title of the expander
 */
void
mx_expander_set_label (MxExpander  *expander,
                       const gchar *label)
{
  g_return_if_fail (MX_IS_EXPANDER (expander));
  g_return_if_fail (label != NULL);

  clutter_text_set_text (CLUTTER_TEXT (expander->priv->label), label);
}

/**
 * mx_expander_set_expanded:
 * @expander: A #MxExpander
 * @expanded: the state of the expander to set
 *
 * Set the state (the #MxExpander:expanded property) of the expander.
 * This will cause the expander to open or close.
 */
void
mx_expander_set_expanded (MxExpander *expander,
                          gboolean    expanded)
{
  g_return_if_fail (MX_IS_EXPANDER (expander));

  if (expander->priv->expanded != expanded)
    {
      expander->priv->expanded = expanded;

      mx_expander_update (expander);

      g_object_notify (G_OBJECT (expander), "expanded");
    }
}

/**
 * mx_expander_get_expanded:
 * @expander: a #MxExpander
 *
 * Get the current state of the expander (the value of #MxExpander:expanded)
 *
 * Returns: #TRUE if the expander is open, #FALSE if it is closed
 */
gboolean
mx_expander_get_expanded (MxExpander *expander)
{
  g_return_val_if_fail (MX_IS_EXPANDER (expander), FALSE);

  return expander->priv->expanded;
}
