/*
 * mx-stack.c: A container that allows the stacking of multiple widgets
 *
 * Copyright 2010 Intel Corporation.
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
 * Written by: Chris Lord <chris@linux.intel.com>
 *
 */

/**
 * SECTION:mx-stack
 * @short_description: A container allow stacking of children over each other
 *
 * The #MxStack arranges its children in a stack, where each child can be
 * allocated its preferred size or larger, if the fill option is set. If
 * the fill option isn't set, a child's position will be determined by its
 * alignment properties.
 *
 * Since: 1.2
 */

#include "mx-stack.h"
#include "mx-stack-child.h"
#include "mx-focusable.h"
#include "mx-utils.h"

#include <string.h>

static void clutter_container_iface_init (ClutterContainerIface *iface);
static void mx_focusable_iface_init (MxFocusableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MxStack, mx_stack, MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init)
                         G_IMPLEMENT_INTERFACE (MX_TYPE_FOCUSABLE,
                                                mx_focusable_iface_init))

#define STACK_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_STACK, MxStackPrivate))

struct _MxStackPrivate
{
  ClutterActor *current_focus;

  ClutterActorBox allocation;
};

/* ClutterContainerIface */
static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->child_meta_type = MX_TYPE_STACK_CHILD;
}

/* MxFocusableIface */

static MxFocusable *
mx_stack_move_focus (MxFocusable     *focusable,
                    MxFocusDirection  direction,
                    MxFocusable      *from)
{
  GList *c, *children;

  MxStackPrivate *priv = MX_STACK (focusable)->priv;

  if (direction == MX_FOCUS_DIRECTION_OUT)
    return NULL;

  children = clutter_actor_get_children (CLUTTER_ACTOR (focusable));

  focusable = NULL;

  c = g_list_find (children, from);
  while (c && !focusable)
    {
      ClutterActor *child;

      switch (direction)
        {
        case MX_FOCUS_DIRECTION_PREVIOUS :
        case MX_FOCUS_DIRECTION_LEFT :
        case MX_FOCUS_DIRECTION_UP :
          c = c->prev;
          break;
        default:
          c = c->next;
          break;
        }

      if (!c)
        continue;

      child = c->data;
      if (!MX_IS_FOCUSABLE (child))
        continue;

      focusable = mx_focusable_accept_focus (MX_FOCUSABLE (child),
                                             MX_FOCUS_HINT_PRIOR);
      if (focusable)
        priv->current_focus = child;
    }

  g_list_free (children);

  return focusable;
}

static MxFocusable *
mx_stack_accept_focus (MxFocusable *focusable, MxFocusHint hint)
{
  GList *c, *children;
  MxStackPrivate *priv = MX_STACK (focusable)->priv;

  children = clutter_actor_get_children (CLUTTER_ACTOR (focusable));
  focusable = NULL;

  switch (hint)
    {
    default:
    case MX_FOCUS_HINT_PRIOR:
      if (priv->current_focus &&
          (!MX_IS_WIDGET (priv->current_focus) ||
           !mx_widget_get_disabled ((MxWidget *)priv->current_focus)))
        {
          focusable =
            mx_focusable_accept_focus (MX_FOCUSABLE (priv->current_focus),
                                       hint);
          if (focusable)
            break;
        }
      /* This purposefully runs into the next case statement */

    case MX_FOCUS_HINT_FIRST:
    case MX_FOCUS_HINT_LAST:
      if (hint == MX_FOCUS_HINT_LAST)
        children = g_list_reverse (children);

      if (children)
        {
          c = children;
          while (c && !focusable)
            {
              ClutterActor *child = c->data;
              c = c->next;

              if (!MX_IS_FOCUSABLE (child))
                continue;

              if (MX_IS_WIDGET (child) &&
                  mx_widget_get_disabled ((MxWidget *)child))
                continue;

              priv->current_focus = child;
              focusable = mx_focusable_accept_focus (MX_FOCUSABLE (child),
                                                     hint);
            }
        }
      break;
    }

  g_list_free (children);
  return focusable;
}

static void
mx_focusable_iface_init (MxFocusableIface *iface)
{
  iface->move_focus = mx_stack_move_focus;
  iface->accept_focus = mx_stack_accept_focus;
}

/* Actor implementation */
static void
mx_stack_get_preferred_width (ClutterActor *actor,
                              gfloat        for_height,
                              gfloat       *min_width_p,
                              gfloat       *nat_width_p)
{
  MxPadding padding;
  gfloat min_width, nat_width, child_min_width, child_nat_width;
  ClutterActorIter iter;
  ClutterActor *child;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (for_height >= 0)
    for_height = MAX (0, for_height - padding.top - padding.bottom);

  min_width = nat_width = 0;
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;
      clutter_actor_get_preferred_width (child, for_height,
                                         &child_min_width, &child_nat_width);
      if (child_min_width > min_width)
        min_width = child_min_width;
      if (child_nat_width > nat_width)
        nat_width = child_nat_width;
    }

  min_width += padding.left + padding.right;
  nat_width += padding.left + padding.right;

  if (min_width_p)
    *min_width_p = min_width;
  if (nat_width_p)
    *nat_width_p = nat_width;
}

static void
mx_stack_get_preferred_height (ClutterActor *actor,
                               gfloat        for_width,
                               gfloat       *min_height_p,
                               gfloat       *nat_height_p)
{
  MxPadding padding;
  gfloat min_height, nat_height, child_min_height, child_nat_height;
  ClutterActorIter iter;
  ClutterActor *child;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (for_width >= 0)
    for_width = MAX (0, for_width - padding.left - padding.right);

  min_height = nat_height = 0;
  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;
      clutter_actor_get_preferred_height (child, for_width,
                                          &child_min_height, &child_nat_height);
      if (child_min_height > min_height)
        min_height = child_min_height;
      if (child_nat_height > nat_height)
        nat_height = child_nat_height;
    }

  min_height += padding.top + padding.bottom;
  nat_height += padding.top + padding.bottom;

  if (min_height_p)
    *min_height_p = min_height;
  if (nat_height_p)
    *nat_height_p = nat_height;
}

static void
mx_stack_allocate (ClutterActor           *actor,
                   const ClutterActorBox  *box,
                   ClutterAllocationFlags  flags)
{
  ClutterActorBox avail_space;
  ClutterActorIter iter;
  ClutterActor *child;

  MxStackPrivate *priv = MX_STACK (actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_stack_parent_class)->allocate (actor, box, flags);

  mx_widget_get_available_area (MX_WIDGET (actor), box, &avail_space);

  memcpy (&priv->allocation, box, sizeof (priv->allocation));

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gboolean x_fill, y_fill, fit, crop;
      MxAlign x_align, y_align;

      ClutterActorBox child_box = avail_space;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      clutter_container_child_get (CLUTTER_CONTAINER (actor),
                                   child,
                                   "x-fill", &x_fill,
                                   "y-fill", &y_fill,
                                   "x-align", &x_align,
                                   "y-align", &y_align,
                                   "fit", &fit,
                                   "crop", &crop,
                                   NULL);

      /* when "crop" is set, fit and fill properties are ignored */
      if (crop)
        {
          gfloat available_height, available_width;
          gfloat natural_width, natural_height;
          gfloat ratio_width, ratio_height, ratio_child;

          available_width  = avail_space.x2 - avail_space.x1;
          available_height = avail_space.y2 - avail_space.y1;

          clutter_actor_get_preferred_size (child,
                                            NULL, NULL,
                                            &natural_width,
                                            &natural_height);

          ratio_child = natural_width / natural_height;
          ratio_width = available_width / natural_width;
          ratio_height = available_height / natural_height;
          if (ratio_width > ratio_height)
            {
              natural_width = available_width;
              natural_height = natural_width / ratio_child;
            }
          else
            {
              natural_height = available_height;
              natural_width = ratio_child * natural_height;
            }

          child_box.x1 = (available_width - natural_width) / 2;
          child_box.y1 = (available_height - natural_height) / 2;
          child_box.x2 = natural_width;
          child_box.y2 = natural_height;

          clutter_actor_allocate (child, &child_box, flags);
          continue;
        }

      /* when "fit" is set, fill properties are ignored */
      if (fit)
        {
          gfloat available_height, available_width, width, height;
          gfloat min_width, natural_width, min_height, natural_height;
          ClutterRequestMode request_mode;

          available_height = avail_space.y2 - avail_space.y1;
          available_width  = avail_space.x2 - avail_space.x1;
          request_mode = clutter_actor_get_request_mode (child);

          if (request_mode == CLUTTER_REQUEST_HEIGHT_FOR_WIDTH)
            {
              clutter_actor_get_preferred_width (child, available_height,
                                                 &min_width,
                                                 &natural_width);
              width = CLAMP (natural_width, min_width, available_width);

              clutter_actor_get_preferred_height (child, width,
                                                  &min_height,
                                                  &natural_height);
              height = CLAMP (natural_height, min_height, available_height);
            }
          else
            {
              clutter_actor_get_preferred_height (child, available_width,
                                                  &min_height,
                                                  &natural_height);
              height = CLAMP (natural_height, min_height, available_height);

              clutter_actor_get_preferred_width (child, height,
                                                 &min_width,
                                                 &natural_width);
              width = CLAMP (natural_width, min_width, available_width);
            }

          child_box.x1 = 0;
          child_box.y1 = 0;

          switch (x_align)
            {
            case MX_ALIGN_START:
              break;
            case MX_ALIGN_MIDDLE:
              child_box.x1 += (gint)(available_width / 2 - width / 2);
              break;

            case MX_ALIGN_END:
              child_box.x1 = avail_space.x2 - width;
              break;
            }

          switch (y_align)
            {
            case MX_ALIGN_START:
              break;
            case MX_ALIGN_MIDDLE:
              child_box.y1 += (gint)(available_height / 2 - height / 2);
              break;

            case MX_ALIGN_END:
              child_box.y1 = avail_space.y2 - height;
              break;
            }

          child_box.x2 = child_box.x1 + width;
          child_box.y2 = child_box.y1 + height;

          clutter_actor_allocate (child, &child_box, flags);
          continue;
        }

      /* Adjust the available space when not filling, otherwise
       * actors that support width-for-height or height-for-width
       * allocation won't shrink correctly.
       */
      if (!x_fill)
        {
          gfloat width;

          clutter_actor_get_preferred_width (child, -1, NULL, &width);

          switch (x_align)
            {
            case MX_ALIGN_START:
              break;
            case MX_ALIGN_MIDDLE:
              child_box.x1 += (gint)((avail_space.x2 - avail_space.x1) / 2 -
                                     width / 2);
              break;

            case MX_ALIGN_END:
              child_box.x1 = avail_space.x2 - width;
              break;
            }

          child_box.x2 = child_box.x1 + width;
          if (child_box.x2 > avail_space.x2)
            child_box.x2 = avail_space.x2;
          if (child_box.x1 < avail_space.x1)
            child_box.x1 = avail_space.x1;
        }

      if (!y_fill)
        {
          gfloat height;

          clutter_actor_get_preferred_height (child, -1, NULL, &height);

          switch (y_align)
            {
            case MX_ALIGN_START:
              break;
            case MX_ALIGN_MIDDLE:
              child_box.y1 += (gint)((avail_space.y2 - avail_space.y1) / 2 -
                                     height / 2);
              break;

            case MX_ALIGN_END:
              child_box.y1 = avail_space.y2 - height;
              break;
            }

          child_box.y2 = child_box.y1 + height;
          if (child_box.y2 > avail_space.y2)
            child_box.y2 = avail_space.y2;
          if (child_box.y1 < avail_space.y1)
            child_box.y1 = avail_space.y1;
        }

      mx_allocate_align_fill (child,
                              &child_box,
                              x_align,
                              y_align,
                              x_fill,
                              y_fill);

      clutter_actor_allocate (child, &child_box, flags);
    }
}

static void
mx_stack_paint_children (ClutterActor *actor)
{
  MxStackPrivate *priv = MX_STACK (actor)->priv;
  ClutterActorIter iter;
  ClutterActor *child;

  clutter_actor_iter_init (&iter, actor);
  while (clutter_actor_iter_next (&iter, &child))
    {
      gboolean crop;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      clutter_container_child_get (CLUTTER_CONTAINER (actor),
                                   child,
                                   "crop", &crop,
                                   NULL);

      if (crop)
        {
          /* clip */
          cogl_clip_push_rectangle (priv->allocation.x1,
                                    priv->allocation.y1,
                                    priv->allocation.x2,
                                    priv->allocation.y2);
          clutter_actor_paint (child);
          cogl_clip_pop ();
        }
      else
        clutter_actor_paint (child);
    }
}

static void
mx_stack_paint (ClutterActor *actor)
{
  /* allow MxWidget to paint the background */
  CLUTTER_ACTOR_CLASS (mx_stack_parent_class)->paint (actor);


  mx_stack_paint_children (actor);
}

static void
mx_stack_pick (ClutterActor       *actor,
               const ClutterColor *color)
{
  CLUTTER_ACTOR_CLASS (mx_stack_parent_class)->pick (actor, color);

  mx_stack_paint_children (actor);
}

static void
mx_stack_class_init (MxStackClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MxStackPrivate));

  actor_class->get_preferred_width = mx_stack_get_preferred_width;
  actor_class->get_preferred_height = mx_stack_get_preferred_height;
  actor_class->allocate = mx_stack_allocate;
  actor_class->paint = mx_stack_paint;
  actor_class->pick = mx_stack_pick;
}

static void
mx_stack_init (MxStack *self)
{
  self->priv = STACK_PRIVATE (self);
}

/**
 * mx_stack_new:
 *
 * Create a new #MxStack.
 *
 * Returns: a newly allocated #MxStack
 *
 * Since: 1.2
 */
ClutterActor *
mx_stack_new ()
{
  return g_object_new (MX_TYPE_STACK, NULL);
}
