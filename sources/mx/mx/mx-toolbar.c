/*
 * mx-toolbar.c: toolbar actor
 *
 * Copyright 2009, 2012 Intel Corporation
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

/**
 * SECTION:mx-toolbar
 * @short_description: A toolbar widget
 *
 * An #MxToolbar is an area that contains items at the top of an #MxWindow.
 * It can optionally include a close button that will close the window.
 */



#include "mx-toolbar.h"
#include "mx-private.h"
#include "mx-marshal.h"
#include <clutter/clutter.h>

static void mx_focusable_iface_init (MxFocusableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MxToolbar, mx_toolbar, MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_FOCUSABLE,
                                                mx_focusable_iface_init))

#define TOOLBAR_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_TOOLBAR, MxToolbarPrivate))


#define SPACING 10

enum {
  PROP_CLOSE_BUTTON = 1
};

enum {
  CLOSE_BUTTON_CLICKED,

  LAST_SIGNAL
};

static guint toolbar_signals[LAST_SIGNAL] = { 0, };

struct _MxToolbarPrivate
{
  guint has_close_button : 1;
  guint child_has_focus  : 1;

  ClutterActor *close_button;

  ClutterActor *child;
};


static MxFocusable *
mx_toolbar_move_focus (MxFocusable      *self,
                       MxFocusDirection  direction,
                       MxFocusable      *from)
{
  MxFocusable *focusable;
  MxToolbarPrivate *priv = MX_TOOLBAR (self)->priv;

  if (priv->child && !MX_IS_FOCUSABLE (priv->child))
    priv->child = NULL;

  focusable = NULL;
  switch (direction)
    {
    case MX_FOCUS_DIRECTION_LEFT:
    case MX_FOCUS_DIRECTION_PREVIOUS:
      if (!priv->child_has_focus && priv->child)
        {
          priv->child_has_focus = TRUE;
          focusable = mx_focusable_accept_focus (MX_FOCUSABLE (priv->child),
                                                 MX_FOCUS_HINT_LAST);
        }
      break;

    case MX_FOCUS_DIRECTION_RIGHT:
    case MX_FOCUS_DIRECTION_NEXT:
      if (priv->child_has_focus && priv->has_close_button)
        {
          priv->child_has_focus = FALSE;
          focusable =
            mx_focusable_accept_focus (MX_FOCUSABLE (priv->close_button),
                                       MX_FOCUS_HINT_FIRST);
        }

    default:
      break;
    }

  return focusable;
}

static MxFocusable *
mx_toolbar_accept_focus (MxFocusable *self,
                         MxFocusHint  hint)
{
  MxFocusable *focusable;
  MxToolbarPrivate *priv = MX_TOOLBAR (self)->priv;

  if (priv->child && !MX_IS_FOCUSABLE (priv->child))
    priv->child = NULL;

  focusable = NULL;
  switch (hint)
    {
    default:
    case MX_FOCUS_HINT_PRIOR:
      if (priv->child && priv->child_has_focus)
        focusable = mx_focusable_accept_focus (MX_FOCUSABLE (priv->child), hint);
      if (focusable)
        break;

    case MX_FOCUS_HINT_LAST:
      priv->child_has_focus = FALSE;
      if (priv->has_close_button)
        focusable =
          mx_focusable_accept_focus (MX_FOCUSABLE (priv->close_button), hint);
      if (focusable)
        break;

    case MX_FOCUS_HINT_FIRST:
      priv->child_has_focus = TRUE;
      if (priv->child)
        focusable = mx_focusable_accept_focus (MX_FOCUSABLE (priv->child), hint);
      break;
    }

  return focusable;
}

static void
mx_focusable_iface_init (MxFocusableIface *iface)
{
  iface->move_focus = mx_toolbar_move_focus;
  iface->accept_focus = mx_toolbar_accept_focus;
}

static void
mx_toolbar_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (object)->priv;

  switch (property_id)
    {
  case PROP_CLOSE_BUTTON:
    g_value_set_boolean (value, priv->has_close_button);
    break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_toolbar_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  switch (property_id)
    {
  case PROP_CLOSE_BUTTON:
    mx_toolbar_set_has_close_button (MX_TOOLBAR (object),
                                     g_value_get_boolean (value));
    break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_toolbar_dispose (GObject *object)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (object)->priv;

  if (priv->close_button)
    {
      clutter_actor_remove_child (CLUTTER_ACTOR (object), priv->close_button);
      priv->close_button = NULL;
    }

  G_OBJECT_CLASS (mx_toolbar_parent_class)->dispose (object);
}

static void
mx_toolbar_finalize (GObject *object)
{
  G_OBJECT_CLASS (mx_toolbar_parent_class)->finalize (object);
}

static void
mx_toolbar_get_preferred_width (ClutterActor *actor,
                                gfloat        for_height,
                                gfloat       *min_width,
                                gfloat       *pref_width)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (actor)->priv;
  MxPadding padding;
  gfloat min_child, pref_child, min_close, pref_close;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  for_height = for_height - padding.top - padding.bottom;

  if (priv->has_close_button && priv->close_button)
    {
      clutter_actor_get_preferred_width (priv->close_button,
                                         for_height,
                                         &min_close,
                                         &pref_close);
    }
  else
    {
      min_close = 0;
      pref_close = 0;
    }

  if (priv->child)
    {
      clutter_actor_get_preferred_width (priv->child,
                                         for_height,
                                         &min_child,
                                         &pref_child);
    }
  else
    {
      min_child = 0;
      pref_child = 0;
    }

  if (min_width)
    *min_width = padding.left + padding.right + min_close + min_child + SPACING;

  if (pref_width)
    *pref_width = padding.left + padding.right + pref_close + pref_child + SPACING;
}

static void
mx_toolbar_get_preferred_height (ClutterActor *actor,
                                 gfloat        for_width,
                                 gfloat       *min_height,
                                 gfloat       *pref_height)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (actor)->priv;
  MxPadding padding;
  gfloat min_child, pref_child, min_close, pref_close;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  if (priv->has_close_button && priv->close_button)
    {
      clutter_actor_get_preferred_height (priv->close_button,
                                          -1,
                                          &min_close,
                                          &pref_close);
    }
  else
    {
      min_close = 0;
      pref_close = 0;
    }

  if (priv->child)
    {
      clutter_actor_get_preferred_height (priv->child,
                                          -1,
                                          &min_child,
                                          &pref_child);
    }
  else
    {
      min_child = 0;
      pref_child = 0;
    }

  if (min_height)
    *min_height = padding.top + padding.bottom + MAX (min_close, min_child);

  if (pref_height)
    *pref_height = padding.top + padding.bottom + MAX (pref_close, pref_child);
}

static void
mx_toolbar_allocate (ClutterActor           *actor,
                     const ClutterActorBox  *box,
                     ClutterAllocationFlags  flags)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (actor)->priv;
  ClutterActorBox childbox, avail;
  gfloat close_w;

  CLUTTER_ACTOR_CLASS (mx_toolbar_parent_class)->allocate (actor, box, flags);

  mx_widget_get_available_area (MX_WIDGET (actor), box, &avail);

  if (priv->close_button)
    {
      gfloat pref_h;

      clutter_actor_get_preferred_size (priv->close_button,
                                        NULL, NULL, &close_w, &pref_h);
      childbox.x1 = avail.x2 - close_w;
      childbox.y1 = avail.y1;
      childbox.x2 = avail.x2;
      childbox.y2 = avail.y2;

      clutter_actor_allocate (priv->close_button, &childbox, flags);
    }
  else
    {
      close_w = 0;
    }

  if (priv->child)
    {
      childbox.x1 = avail.x1;
      childbox.y1 = avail.y1;
      childbox.x2 = MAX (childbox.x1, avail.x2 - close_w - SPACING);
      childbox.y2 = MAX (childbox.y1, avail.y2);

      clutter_actor_allocate (priv->child, &childbox, flags);
    }
}

static void
mx_toolbar_pick (ClutterActor       *actor,
                 const ClutterColor *color)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_toolbar_parent_class)->pick (actor, color);

  if (priv->child)
    clutter_actor_paint (priv->child);

  if (priv->close_button
      && clutter_actor_should_pick_paint (priv->close_button))
    clutter_actor_paint (priv->close_button);
}

static void
mx_toolbar_paint (ClutterActor *actor)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_toolbar_parent_class)->paint (actor);

  if (priv->child)
    clutter_actor_paint (priv->child);

  if (priv->close_button)
    clutter_actor_paint (priv->close_button);
}

static void
mx_toolbar_class_init (MxToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MxToolbarPrivate));

  object_class->get_property = mx_toolbar_get_property;
  object_class->set_property = mx_toolbar_set_property;
  object_class->dispose = mx_toolbar_dispose;
  object_class->finalize = mx_toolbar_finalize;

  actor_class->get_preferred_width = mx_toolbar_get_preferred_width;
  actor_class->get_preferred_height = mx_toolbar_get_preferred_height;
  actor_class->allocate = mx_toolbar_allocate;
  actor_class->pick = mx_toolbar_pick;
  actor_class->paint = mx_toolbar_paint;

  /**
   * MxToolbar::close-button-clicked:
   *
   * Emitted when the close button of the toolbar is clicked.
   *
   * Normally, the parent stage will be closed when the close button is
   * clicked. Return #TRUE from this handler to prevent the stage from being
   * destroyed.
   *
   * Returns: #TRUE to prevent the parent stage being destroyed.
   */
  pspec = g_param_spec_boolean ("has-close-button",
                                "Has Close Button",
                                "Whether to show a close button on the toolbar",
                                TRUE,
                                MX_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CLOSE_BUTTON, pspec);

  toolbar_signals[CLOSE_BUTTON_CLICKED] =
    g_signal_new ("close-button-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MxToolbarClass, close_button_clicked),
                  NULL, NULL,
                  _mx_marshal_BOOL__VOID,
                  G_TYPE_BOOLEAN, 0);


}

static void
mx_toolbar_actor_added (ClutterActor *container,
                        ClutterActor *actor)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (container)->priv;

  if (MX_IS_TOOLTIP (actor))
    return;

  if (priv->child)
    clutter_actor_remove_child (container, priv->child);

  priv->child = actor;
}

static void
mx_toolbar_actor_removed (ClutterActor *container,
                          ClutterActor *actor)
{
  MxToolbarPrivate *priv = MX_TOOLBAR (container)->priv;

  if (priv->child == actor)
    priv->child = NULL;
}


static void
mx_toolbar_init (MxToolbar *self)
{
  self->priv = TOOLBAR_PRIVATE (self);

  mx_toolbar_set_has_close_button (self, TRUE);

  g_signal_connect (self, "actor-added", G_CALLBACK (mx_toolbar_actor_added),
                    NULL);
  g_signal_connect (self, "actor-removed",
                    G_CALLBACK (mx_toolbar_actor_removed), NULL);
}

static void
close_button_click_cb (MxButton  *button,
                       MxToolbar *toolbar)
{
  gboolean handled;
  g_signal_emit (toolbar, toolbar_signals[CLOSE_BUTTON_CLICKED], 0, &handled);

  if (!handled)
    clutter_actor_destroy (clutter_actor_get_stage (CLUTTER_ACTOR (toolbar)));
}

/**
 * mx_toolbar_new:
 *
 * Create a new #MxToolbar. This is not normally necessary if using #MxWindow,
 * where #mx_window_get_toolbar should be used to retrieve the toolbar instead.
 *
 * Returns: A newly allocated #MxToolbar
 */
ClutterActor *
mx_toolbar_new (void)
{
  return g_object_new (MX_TYPE_TOOLBAR, NULL);
}


/**
 * mx_toolbar_set_has_close_button:
 * @toolbar: A #MxToolbar
 * @has_close_button: #TRUE if a close button should be displayed
 *
 * Set the #MxToolbar:has-close-button property
 *
 */
void
mx_toolbar_set_has_close_button (MxToolbar *toolbar,
                                 gboolean   has_close_button)
{
  MxToolbarPrivate *priv;

  g_return_if_fail (MX_IS_TOOLBAR (toolbar));

  priv = toolbar->priv;

  if (priv->has_close_button != has_close_button)
    {
      priv->has_close_button = has_close_button;

      if (!has_close_button)
        {
          if (priv->close_button)
            {
              clutter_actor_destroy (priv->close_button);
              priv->close_button = NULL;
            }
        }
      else
        {
          priv->close_button = mx_button_new ();
          clutter_actor_set_name (priv->close_button, "close-button");
          clutter_actor_add_child (CLUTTER_ACTOR (toolbar),
                                   priv->close_button);
          g_signal_connect (priv->close_button, "clicked",
                            G_CALLBACK (close_button_click_cb), toolbar);
          mx_stylable_style_changed (MX_STYLABLE (priv->close_button),
                                     MX_STYLE_CHANGED_FORCE);
        }

      clutter_actor_queue_relayout (CLUTTER_ACTOR (toolbar));

      g_object_notify (G_OBJECT (toolbar), "has-close-button");
    }
}

/**
 * mx_toolbar_get_has_close_button:
 * @toolbar: A #MxToolbar
 *
 * Get the value of the #MxToolbar:has-close-button property.
 *
 * Returns: the current value of the "hast-close-button" property.
 */
gboolean
mx_toolbar_get_has_close_button (MxToolbar *toolbar)
{
  g_return_val_if_fail (MX_IS_TOOLBAR (toolbar), FALSE);

  return toolbar->priv->has_close_button;
}
