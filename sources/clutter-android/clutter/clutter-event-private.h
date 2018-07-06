#ifndef __CLUTTER_EVENT_PRIVATE_H__
#define __CLUTTER_EVENT_PRIVATE_H__

#include <clutter/clutter-event.h>

G_BEGIN_DECLS

void            _clutter_event_set_pointer_emulated     (ClutterEvent       *event,
                                                         gboolean            is_emulated);

/* Reinjecting queued events for processing */
void            _clutter_process_event                  (ClutterEvent       *event);

/* clears the event queue inside the main context */
void            _clutter_clear_events_queue             (void);
void            _clutter_clear_events_queue_for_stage   (ClutterStage       *stage);

void            _clutter_event_set_platform_data        (ClutterEvent       *event,
                                                         gpointer            data);
gpointer        _clutter_event_get_platform_data        (const ClutterEvent *event);

void            _clutter_event_push                     (const ClutterEvent *event,
                                                         gboolean            do_copy);

G_END_DECLS

#endif /* __CLUTTER_EVENT_PRIVATE_H__ */
