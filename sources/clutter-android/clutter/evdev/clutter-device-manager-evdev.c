/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corp.
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
 * Author: Damien Lespiau <damien.lespiau@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gudev/gudev.h>

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-manager.h"
#include "clutter-xkb-utils.h"
#include "clutter-backend-private.h"
#include "clutter-evdev.h"

#include "clutter-device-manager-evdev.h"

#define CLUTTER_DEVICE_MANAGER_EVDEV_GET_PRIVATE(obj)               \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                              \
                                CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,  \
                                ClutterDeviceManagerEvdevPrivate))

G_DEFINE_TYPE (ClutterDeviceManagerEvdev,
               clutter_device_manager_evdev,
               CLUTTER_TYPE_DEVICE_MANAGER);

struct _ClutterDeviceManagerEvdevPrivate
{
  GUdevClient *udev_client;

  ClutterStage *stage;
  gboolean released;

  GSList *devices;          /* list of ClutterInputDeviceEvdevs */
  GSList *event_sources;    /* list of the event sources */

  ClutterInputDevice *core_pointer;
  ClutterInputDevice *core_keyboard;

  ClutterStageManager *stage_manager;
  guint stage_added_handler;
  guint stage_removed_handler;
};

static const gchar *subsystems[] = { "input", NULL };

/*
 * ClutterEventSource management
 *
 * The device manager is responsible for managing the GSource when devices
 * appear and disappear from the system.
 *
 * FIXME: For now, we associate a GSource with every single device. Starting
 * from glib 2.28 we can use g_source_add_child_source() to have a single
 * GSource for the device manager, each device becoming a child source. Revisit
 * this once we depend on glib >= 2.28.
 */


static const char *option_xkb_layout = "us";
static const char *option_xkb_variant = "";
static const char *option_xkb_options = "";

/*
 * ClutterEventSource for reading input devices
 */

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterInputDeviceEvdev *device;    /* back pointer to the evdev device */
  GPollFD event_poll_fd;              /* file descriptor of the /dev node */
  struct xkb_state *xkb;              /* XKB state object */
  gint x, y;                          /* last x, y position for pointers */
  guint32 modifier_state;             /* key modifiers */
};

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;
  retval = clutter_events_pending ();

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  retval = ((event_source->event_poll_fd.revents & G_IO_IN) ||
            clutter_events_pending ());

  _clutter_threads_release_lock ();

  return retval;
}

static void
queue_event (ClutterEvent *event)
{
  if (event == NULL)
    return;

  _clutter_event_push (event, FALSE);
}

static void
notify_key (ClutterEventSource *source,
            guint32             time_,
            guint32             key,
            guint32             state)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  ClutterStage *stage;
  ClutterEvent *event = NULL;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  /* if we have a mapping for that device, use it to generate the event */
  if (source->xkb)
  {
    event =
      _clutter_key_event_new_from_evdev (input_device,
                                         stage,
                                         source->xkb,
                                         time_, key, state);
    xkb_state_update_key (source->xkb, key, state ? XKB_KEY_DOWN : XKB_KEY_UP);
  }

  queue_event (event);
}


static void
notify_motion (ClutterEventSource *source,
               guint32             time_,
               gint                x,
               gint                y)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  gfloat stage_width, stage_height, new_x, new_y;
  ClutterEvent *event;
  ClutterStage *stage;

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  stage_width = clutter_actor_get_width (CLUTTER_ACTOR (stage));
  stage_height = clutter_actor_get_height (CLUTTER_ACTOR (stage));

  event = clutter_event_new (CLUTTER_MOTION);

  if (x < 0)
    new_x = 0.f;
  else if (x >= stage_width)
    new_x = stage_width - 1;
  else
    new_x = x;

  if (y < 0)
    new_y = 0.f;
  else if (y >= stage_height)
    new_y = stage_height - 1;
  else
    new_y = y;

  source->x = new_x;
  source->y = new_y;

  event->motion.time = time_;
  event->motion.stage = stage;
  event->motion.device = input_device;
  event->motion.modifier_state = source->modifier_state;
  event->motion.x = new_x;
  event->motion.y = new_y;

  queue_event (event);
}

static void
notify_button (ClutterEventSource *source,
               guint32             time_,
               guint32             button,
               guint32             state)
{
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  ClutterEvent *event;
  ClutterStage *stage;
  gint button_nr;
  static gint maskmap[8] =
    {
      CLUTTER_BUTTON1_MASK, CLUTTER_BUTTON3_MASK, CLUTTER_BUTTON2_MASK,
      CLUTTER_BUTTON4_MASK, CLUTTER_BUTTON5_MASK, 0, 0, 0
    };

  /* We can drop the event on the floor if no stage has been
   * associated with the device yet. */
  stage = _clutter_input_device_get_stage (input_device);
  if (!stage)
    return;

  /* The evdev button numbers don't map sequentially to clutter button
   * numbers (the right and middle mouse buttons are in the opposite
   * order) so we'll map them directly with a switch statement */
  switch (button)
    {
    case BTN_LEFT:
      button_nr = CLUTTER_BUTTON_PRIMARY;
      break;

    case BTN_RIGHT:
      button_nr = CLUTTER_BUTTON_SECONDARY;
      break;

    case BTN_MIDDLE:
      button_nr = CLUTTER_BUTTON_MIDDLE;
      break;

    default:
      button_nr = button - BTN_MOUSE + 1;
      break;
    }

  if (G_UNLIKELY (button_nr < 1 || button_nr > 8))
    {
      g_warning ("Unhandled button event 0x%x", button);
      return;
    }

  if (state)
    event = clutter_event_new (CLUTTER_BUTTON_PRESS);
  else
    event = clutter_event_new (CLUTTER_BUTTON_RELEASE);

  /* Update the modifiers */
  if (state)
    source->modifier_state |= maskmap[button - BTN_LEFT];
  else
    source->modifier_state &= ~maskmap[button - BTN_LEFT];

  event->button.time = time_;
  event->button.stage = CLUTTER_STAGE (stage);
  event->button.device = (ClutterInputDevice *) source->device;
  event->button.modifier_state = source->modifier_state;
  event->button.button = button_nr;
  event->button.x = source->x;
  event->button.y = source->y;

  queue_event (event);
}

static gboolean
clutter_event_dispatch (GSource     *g_source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEventSource *source = (ClutterEventSource *) g_source;
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  struct input_event ev[8];
  ClutterEvent *event;
  gint len, i, dx = 0, dy = 0;
  uint32_t _time;
  ClutterStage *stage;

  _clutter_threads_acquire_lock ();

  stage = _clutter_input_device_get_stage (input_device);

  /* Don't queue more events if we haven't finished handling the previous batch
   */
  if (!clutter_events_pending ())
    {
       len = read (source->event_poll_fd.fd, &ev, sizeof (ev));
       if (len < 0 || len % sizeof (ev[0]) != 0)
       {
         if (errno != EAGAIN)
           {
             ClutterDeviceManager *manager;
             ClutterInputDevice *device;
             const gchar *device_path;

             device = CLUTTER_INPUT_DEVICE (source->device);

             if (CLUTTER_HAS_DEBUG (EVENT))
               {
                 device_path =
                   _clutter_input_device_evdev_get_device_path (source->device);

                 CLUTTER_NOTE (EVENT, "Could not read device (%s), removing.",
                               device_path);
               }

             /* remove the faulty device */
             manager = clutter_device_manager_get_default ();
             _clutter_device_manager_remove_device (manager, device);

           }
         goto out;
       }

       /* Drop events if we don't have any stage to forward them to */
       if (!stage)
         goto out;

       for (i = 0; i < len / sizeof (ev[0]); i++)
         {
           struct input_event *e = &ev[i];

           _time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;
           event = NULL;

           switch (e->type)
             {
             case EV_KEY:

               /* don't repeat mouse buttons */
               if (e->code >= BTN_MOUSE && e->code < KEY_OK)
                 if (e->value == 2)
                   continue;

               switch (e->code)
                 {
                 case BTN_TOUCH:
                 case BTN_TOOL_PEN:
                 case BTN_TOOL_RUBBER:
                 case BTN_TOOL_BRUSH:
                 case BTN_TOOL_PENCIL:
                 case BTN_TOOL_AIRBRUSH:
                 case BTN_TOOL_FINGER:
                 case BTN_TOOL_MOUSE:
                 case BTN_TOOL_LENS:
                   break;

                 case BTN_LEFT:
                 case BTN_RIGHT:
                 case BTN_MIDDLE:
                 case BTN_SIDE:
                 case BTN_EXTRA:
                 case BTN_FORWARD:
                 case BTN_BACK:
                 case BTN_TASK:
                   notify_button(source, _time, e->code, e->value);
                   break;

                 default:
                   notify_key (source, _time, e->code, e->value);
                 break;
                 }
               break;

             case EV_SYN:
               /* Nothing to do here? */
               break;

             case EV_MSC:
               /* Nothing to do here? */
               break;

             case EV_REL:
               /* compress the EV_REL events in dx/dy */
               switch (e->code)
                 {
                 case REL_X:
                   dx += e->value;
                   break;
                 case REL_Y:
                   dy += e->value;
                   break;
                 }
               break;

             case EV_ABS:
             default:
               g_warning ("Unhandled event of type %d", e->type);
               break;
             }

           queue_event (event);
         }

       if (dx != 0 || dy != 0)
         notify_motion (source, _time, source->x + dx, source->y + dy);
    }

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);
    }

out:
  _clutter_threads_release_lock ();

  return TRUE;
}
static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

static GSource *
clutter_event_source_new (ClutterInputDeviceEvdev *input_device)
{
  GSource *source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  ClutterInputDeviceType type;
  const gchar *node_path;
  gint fd;

  /* grab the udev input device node and open it */
  node_path = _clutter_input_device_evdev_get_device_path (input_device);

  CLUTTER_NOTE (EVENT, "Creating GSource for device %s", node_path);

  fd = open (node_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      g_warning ("Could not open device %s: %s", node_path, strerror (errno));
      return NULL;
    }

  /* setup the source */
  event_source->device = input_device;
  event_source->event_poll_fd.fd = fd;
  event_source->event_poll_fd.events = G_IO_IN;

  type =
    clutter_input_device_get_device_type (CLUTTER_INPUT_DEVICE (input_device));

  if (type == CLUTTER_KEYBOARD_DEVICE)
    {
      /* create the xkb description */
      event_source->xkb = _clutter_xkb_state_new (NULL,
                                                  option_xkb_layout,
                                                  option_xkb_variant,
                                                  option_xkb_options);
      if (G_UNLIKELY (event_source->xkb == NULL))
        {
          g_warning ("Could not compile keymap %s:%s:%s", option_xkb_layout,
                     option_xkb_variant, option_xkb_options);
          close (fd);
          g_source_unref (source);
          return NULL;
        }
    }
  else if (type == CLUTTER_POINTER_DEVICE)
    {
      event_source->x = 0;
      event_source->y = 0;
    }

  /* and finally configure and attach the GSource */
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

static void
clutter_event_source_free (ClutterEventSource *source)
{
  GSource *g_source = (GSource *) source;
  const gchar *node_path;

  node_path = _clutter_input_device_evdev_get_device_path (source->device);

  CLUTTER_NOTE (EVENT, "Removing GSource for device %s", node_path);

  /* ignore the return value of close, it's not like we can do something
   * about it */
  close (source->event_poll_fd.fd);

  g_source_destroy (g_source);
  g_source_unref (g_source);
}

static ClutterEventSource *
find_source_by_device (ClutterDeviceManagerEvdev *manager,
                       ClutterInputDevice        *device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager->priv;
  GSList *l;

  for (l = priv->event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      if (source->device == (ClutterInputDeviceEvdev *) device)
        return source;
    }

  return NULL;
}

static gboolean
is_evdev (const gchar *sysfs_path)
{
  GRegex *regex;
  gboolean match;

  regex = g_regex_new ("/input[0-9]+/event[0-9]+$", 0, 0, NULL);
  match = g_regex_match (regex, sysfs_path, 0, NULL);

  g_regex_unref (regex);
  return match;
}

static void
evdev_add_device (ClutterDeviceManagerEvdev *manager_evdev,
                  GUdevDevice               *udev_device)
{
  ClutterDeviceManager *manager = (ClutterDeviceManager *) manager_evdev;
  ClutterInputDeviceType type = CLUTTER_EXTENSION_DEVICE;
  ClutterInputDevice *device;
  const gchar *device_file, *sysfs_path, *device_name;

  device_file = g_udev_device_get_device_file (udev_device);
  sysfs_path = g_udev_device_get_sysfs_path (udev_device);
  device_name = g_udev_device_get_name (udev_device);

  if (device_file == NULL || sysfs_path == NULL)
    return;

  if (g_udev_device_get_property (udev_device, "ID_INPUT") == NULL)
    return;

  /* Make sure to only add evdev devices, ie the device with a sysfs path that
   * finishes by input%d/event%d (We don't rely on the node name as this
   * policy is enforced by udev rules Vs API/ABI guarantees of sysfs) */
  if (!is_evdev (sysfs_path))
    return;

  /* Clutter assumes that device types are exclusive in the
   * ClutterInputDevice API */
  if (g_udev_device_has_property (udev_device, "ID_INPUT_KEYBOARD"))
    type = CLUTTER_KEYBOARD_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_MOUSE"))
    type = CLUTTER_POINTER_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_JOYSTICK"))
    type = CLUTTER_JOYSTICK_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TABLET"))
    type = CLUTTER_TABLET_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TOUCHPAD"))
    type = CLUTTER_TOUCHPAD_DEVICE;
  else if (g_udev_device_has_property (udev_device, "ID_INPUT_TOUCHSCREEN"))
    type = CLUTTER_TOUCHSCREEN_DEVICE;

  device = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_EVDEV,
                         "id", 0,
                         "name", device_name,
                         "device-type", type,
                         "sysfs-path", sysfs_path,
                         "device-path", device_file,
                         "enabled", TRUE,
                         NULL);

  _clutter_input_device_set_stage (device, manager_evdev->priv->stage);

  _clutter_device_manager_add_device (manager, device);

  CLUTTER_NOTE (EVENT, "Added device %s, type %d, sysfs %s",
                device_file, type, sysfs_path);
}

static ClutterInputDeviceEvdev *
find_device_by_udev_device (ClutterDeviceManagerEvdev *manager_evdev,
                            GUdevDevice               *udev_device)
{
  ClutterDeviceManagerEvdevPrivate *priv = manager_evdev->priv;
  GSList *l;
  const gchar *sysfs_path;

  sysfs_path = g_udev_device_get_sysfs_path (udev_device);
  if (sysfs_path == NULL)
    {
      g_message ("device file is NULL");
      return NULL;
    }

  for (l = priv->devices; l; l = g_slist_next (l))
    {
      ClutterInputDeviceEvdev *device = l->data;

      if (strcmp (sysfs_path,
                  _clutter_input_device_evdev_get_sysfs_path (device)) == 0)
        {
          return device;
        }
    }

  return NULL;
}

static void
evdev_remove_device (ClutterDeviceManagerEvdev *manager_evdev,
                     GUdevDevice               *device)
{
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (manager_evdev);
  ClutterInputDeviceEvdev *device_evdev;
  ClutterInputDevice *input_device;

  device_evdev = find_device_by_udev_device (manager_evdev, device);
  if (device_evdev == NULL)
      return;

  input_device = CLUTTER_INPUT_DEVICE (device_evdev);
  _clutter_device_manager_remove_device (manager, input_device);
}

static void
on_uevent (GUdevClient *client,
           gchar       *action,
           GUdevDevice *device,
           gpointer     data)
{
  ClutterDeviceManagerEvdev *manager = CLUTTER_DEVICE_MANAGER_EVDEV (data);
  ClutterDeviceManagerEvdevPrivate *priv = manager->priv;

  if (priv->released)
    return;

  if (g_strcmp0 (action, "add") == 0)
    evdev_add_device (manager, device);
  else if (g_strcmp0 (action, "remove") == 0)
    evdev_remove_device (manager, device);
}

/*
 * ClutterDeviceManager implementation
 */

static void
clutter_device_manager_evdev_add_device (ClutterDeviceManager *manager,
                                         ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterInputDeviceType device_type;
  ClutterInputDeviceEvdev *device_evdev;
  gboolean is_pointer, is_keyboard;
  GSource *source;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  device_evdev = CLUTTER_INPUT_DEVICE_EVDEV (device);

  device_type = clutter_input_device_get_device_type (device);
  is_pointer  = device_type == CLUTTER_POINTER_DEVICE;
  is_keyboard = device_type == CLUTTER_KEYBOARD_DEVICE;

  priv->devices = g_slist_prepend (priv->devices, device);

  if (is_pointer && priv->core_pointer == NULL)
    priv->core_pointer = device;

  if (is_keyboard && priv->core_keyboard == NULL)
    priv->core_keyboard = device;

  /* Install the GSource for this device */
  source = clutter_event_source_new (device_evdev);
  if (G_LIKELY (source))
    priv->event_sources = g_slist_prepend (priv->event_sources, source);
}

static void
clutter_device_manager_evdev_remove_device (ClutterDeviceManager *manager,
                                            ClutterInputDevice   *device)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  ClutterEventSource *source;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  /* Remove the device */
  priv->devices = g_slist_remove (priv->devices, device);

  /* Remove the source */
  source = find_source_by_device (manager_evdev, device);
  if (G_UNLIKELY (source == NULL))
    {
      g_warning ("Trying to remove a device without a source installed ?!");
      return;
    }

  clutter_event_source_free (source);
  priv->event_sources = g_slist_remove (priv->event_sources, source);
}

static const GSList *
clutter_device_manager_evdev_get_devices (ClutterDeviceManager *manager)
{
  return CLUTTER_DEVICE_MANAGER_EVDEV (manager)->priv->devices;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_core_device (ClutterDeviceManager   *manager,
                                              ClutterInputDeviceType  type)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return priv->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return priv->core_keyboard;

    case CLUTTER_EXTENSION_DEVICE:
    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_evdev_get_device (ClutterDeviceManager *manager,
                                         gint                  id)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = manager_evdev->priv;

  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_id (device) == id)
        return device;
    }

  return NULL;
}

static void
clutter_device_manager_evdev_probe_devices (ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GList *devices, *l;

  devices = g_udev_client_query_by_subsystem (priv->udev_client, subsystems[0]);
  for (l = devices; l; l = g_list_next (l))
    {
      GUdevDevice *device = l->data;

      evdev_add_device (self, device);
      g_object_unref (device);
    }
  g_list_free (devices);
}

/*
 * GObject implementation
 */

static void
clutter_device_manager_evdev_constructed (GObject *gobject)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (gobject);
  priv = manager_evdev->priv;

  priv->udev_client = g_udev_client_new (subsystems);

  clutter_device_manager_evdev_probe_devices (manager_evdev);

  /* subcribe for events on input devices */
  g_signal_connect (priv->udev_client, "uevent",
                    G_CALLBACK (on_uevent), manager_evdev);
}

static void
clutter_device_manager_evdev_dispose (GObject *object)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  if (priv->stage_added_handler)
    {
      g_signal_handler_disconnect (priv->stage_manager,
                                   priv->stage_added_handler);
      priv->stage_added_handler = 0;
    }

  if (priv->stage_removed_handler)
    {
      g_signal_handler_disconnect (priv->stage_manager,
                                   priv->stage_removed_handler);
      priv->stage_removed_handler = 0;
    }

  if (priv->stage_manager)
    {
      g_object_unref (priv->stage_manager);
      priv->stage_manager = NULL;
    }

  G_OBJECT_CLASS (clutter_device_manager_evdev_parent_class)->dispose (object);
}

static void
clutter_device_manager_evdev_finalize (GObject *object)
{
  ClutterDeviceManagerEvdev *manager_evdev;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l;

  manager_evdev = CLUTTER_DEVICE_MANAGER_EVDEV (object);
  priv = manager_evdev->priv;

  g_object_unref (priv->udev_client);

  for (l = priv->devices; l; l = g_slist_next (l))
    {
      ClutterInputDevice *device = l->data;

      g_object_unref (device);
    }
  g_slist_free (priv->devices);

  for (l = priv->event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      clutter_event_source_free (source);
    }
  g_slist_free (priv->event_sources);

  G_OBJECT_CLASS (clutter_device_manager_evdev_parent_class)->finalize (object);
}

static void
clutter_device_manager_evdev_class_init (ClutterDeviceManagerEvdevClass *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (ClutterDeviceManagerEvdevPrivate));

  gobject_class->constructed = clutter_device_manager_evdev_constructed;
  gobject_class->finalize = clutter_device_manager_evdev_finalize;
  gobject_class->dispose = clutter_device_manager_evdev_dispose;

  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_evdev_add_device;
  manager_class->remove_device = clutter_device_manager_evdev_remove_device;
  manager_class->get_devices = clutter_device_manager_evdev_get_devices;
  manager_class->get_core_device = clutter_device_manager_evdev_get_core_device;
  manager_class->get_device = clutter_device_manager_evdev_get_device;
}

static void
clutter_device_manager_evdev_stage_added_cb (ClutterStageManager *manager,
                                             ClutterStage *stage,
                                             ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GSList *l;

  /* NB: Currently we can only associate a single stage with all evdev
   * devices.
   *
   * We save a pointer to the stage so if we release/reclaim input
   * devices due to switching virtual terminals then we know what
   * stage to re associate the devices with.
   */
  priv->stage = stage;

  /* Set the stage of any devices that don't already have a stage */
  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (_clutter_input_device_get_stage (device) == NULL)
        _clutter_input_device_set_stage (device, stage);
    }

  /* We only want to do this once so we can catch the default
     stage. If the application has multiple stages then it will need
     to manage the stage of the input devices itself */
  g_signal_handler_disconnect (priv->stage_manager,
                               priv->stage_added_handler);
  priv->stage_added_handler = 0;
}

static void
clutter_device_manager_evdev_stage_removed_cb (ClutterStageManager *manager,
                                               ClutterStage *stage,
                                               ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv = self->priv;
  GSList *l;

  /* Remove the stage of any input devices that were pointing to this
     stage so we don't send events to invalid stages */
  for (l = priv->devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (_clutter_input_device_get_stage (device) == stage)
        _clutter_input_device_set_stage (device, NULL);
    }
}

static void
clutter_device_manager_evdev_init (ClutterDeviceManagerEvdev *self)
{
  ClutterDeviceManagerEvdevPrivate *priv;

  priv = self->priv = CLUTTER_DEVICE_MANAGER_EVDEV_GET_PRIVATE (self);

  priv->stage_manager = clutter_stage_manager_get_default ();
  g_object_ref (priv->stage_manager);

  /* evdev doesn't have any way to link an event to a particular stage
     so we'll have to leave it up to applications to set the
     corresponding stage for an input device. However to make it
     easier for applications that are only using one fullscreen stage
     (which is probably the most frequent use-case for the evdev
     backend) we'll associate any input devices that don't have a
     stage with the first stage created. */
  priv->stage_added_handler =
    g_signal_connect (priv->stage_manager,
                      "stage-added",
                      G_CALLBACK (clutter_device_manager_evdev_stage_added_cb),
                      self);
  priv->stage_removed_handler =
    g_signal_connect (priv->stage_manager,
                      "stage-removed",
                      G_CALLBACK (clutter_device_manager_evdev_stage_removed_cb),
                      self);
}

void
_clutter_events_evdev_init (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "Initializing evdev backend");

  backend->device_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_EVDEV,
                                          "backend", backend,
                                          NULL);
}

void
_clutter_events_evdev_uninit (ClutterBackend *backend)
{
  CLUTTER_NOTE (EVENT, "Uninitializing evdev backend");
}

/**
 * clutter_evdev_release_devices:
 *
 * Releases all the evdev devices that Clutter is currently managing. This api
 * is typically used when switching away from the Clutter application when
 * switching tty. The devices can be reclaimed later with a call to
 * clutter_evdev_reclaim_devices().
 *
 * This function should only be called after clutter has been initialized.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
clutter_evdev_release_devices (void)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  ClutterDeviceManagerEvdev *evdev_manager;
  ClutterDeviceManagerEvdevPrivate *priv;
  GSList *l, *next;

  if (!manager)
    {
      g_warning ("clutter_evdev_release_devices shouldn't be called "
                 "before clutter_init()");
      return;
    }

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager));

  evdev_manager = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = evdev_manager->priv;

  if (priv->released)
    {
      g_warning ("clutter_evdev_release_devices() shouldn't be called "
                 "multiple times without a corresponding call to "
                 "clutter_evdev_reclaim_devices() first");
      return;
    }

  for (l = priv->devices; l; l = next)
    {
      ClutterInputDevice *device = l->data;

      /* Be careful about the list we're iterating being modified... */
      next = l->next;

      _clutter_device_manager_remove_device (manager, device);
    }

  priv->released = TRUE;
}

/**
 * clutter_evdev_reclaim_devices:
 *
 * This causes Clutter to re-probe for evdev devices. This is must only be
 * called after a corresponding call to clutter_evdev_release_devices()
 * was previously used to release all evdev devices. This API is typically
 * used when a clutter application using evdev has regained focus due to
 * switching ttys.
 *
 * This function should only be called after clutter has been initialized.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
clutter_evdev_reclaim_devices (void)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();
  ClutterDeviceManagerEvdev *evdev_manager;
  ClutterDeviceManagerEvdevPrivate *priv;

  if (!manager)
    {
      g_warning ("clutter_evdev_reclaim_devices shouldn't be called "
                 "before clutter_init()");
      return;
    }

  g_return_if_fail (CLUTTER_IS_DEVICE_MANAGER_EVDEV (manager));

  evdev_manager = CLUTTER_DEVICE_MANAGER_EVDEV (manager);
  priv = evdev_manager->priv;

  if (!priv->released)
    {
      g_warning ("Spurious call to clutter_evdev_reclaim_devices() without "
                 "previous call to clutter_evdev_release_devices");
      return;
    }

  priv->released = FALSE;
  clutter_device_manager_evdev_probe_devices (evdev_manager);
}
