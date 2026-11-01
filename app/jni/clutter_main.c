/*
 * Copyright © 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms and conditions of the GNU Lesser General
 * Public License, version 2.1, as published by the Free Software
 * Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

//#define COGL_ENABLE_EXPERIMENTAL_API

#include <glib.h>
#include <cogl/cogl.h>
#include <clutter/clutter.h>
//#include <clutter/clutter-stage.h>
#ifndef _PC_VERSION
# include <clutter/android/clutter-android.h>
#endif
//#include <mx/mx.h>

# include <clutter/android/clutter-android-application-private.h>

#include "clutter-utils.h"
#include "inferno_storage.h"
#include "inferno_viewport.h"
#include "inferno_ui_queue.h"
#include <limits.h>
#include <math.h>



#include <android/log.h>


#define  LOG_TAG    "inferno CLU"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)




typedef unsigned long ulong;


typedef struct
{
  ClutterAndroidApplication *application;

  ClutterAction *drag_action;
  ClutterAction *rotate_action;
  ClutterAction *zoom_action;
} TestData;


typedef struct ActorLayer ActorLayer;

typedef struct ActorData ActorData;

struct ActorLayer
{
	ClutterActor *main_actor;
	ActorData *main_actor_data;

	ActorData *first_child;

	int layer_id;
	
	ActorLayer *next;
};


struct ActorData
{
	ClutterActor *actor;
	char *buffer_ext;
	char *buffer;
	int w, h;
	int x, y;

	unsigned int is_changed;

	ClutterEffectClass *effect;

	int layer_id;
	ActorLayer *layer;
	ActorData *next_in_layer;
	
	ActorData *next;
};


ActorData *actor_data_root = NULL;
ActorLayer *layer_root = NULL;


int Xsize;
int Ysize;

extern char rootdir[];

char *xscreendata = NULL;

extern void amain();


ClutterActor *stage = NULL;
static ClutterActor *inferno_canvas; // Common logical parent for every Inferno GLES actor.
static IoaViewport inferno_viewport; // Exact stage placement used for both display and input.
static IoaPointer inferno_pointer; // Native touch sequences owned by the Clutter event thread.
ClutterActor *desktop = NULL;
ClutterActor *topmost = NULL;
//ClutterContent *image = NULL;

ClutterTimeline *desktop_timeline = NULL;

struct android_app *aapp = NULL;


int del_flag = 0;



CoglTexture*
cogl_texture_new_from_buffer (const unsigned char *pixels, int width, int height, int bpp);





extern void xmouse_btn(int, int, int);




static void sync_surface_geometry(void);


static ActorData *
find_ActorData (ClutterActor *actor)
{  
	ActorData *actor_data = actor_data_root;
	
	for( ; actor_data != NULL && actor_data->actor != actor; actor_data = actor_data->next);
	
	return actor_data;
}



static void
_update_progress_cb (ClutterTimeline *timeline,
                     guint            elapsed_msecs,
                     ClutterTexture  *texture)
{
//  CoglHandle copy;
  gdouble progress;
//  CoglColor constant;

//  CoglHandle material = clutter_texture_get_cogl_material (texture);

  ActorData *actor_data = actor_data_root;
  char *buffer = NULL;
  int w, h;


//  if (material == COGL_INVALID_HANDLE)
//    return;

  /* You should assume that a material can only be modified once, after
   * its creation; if you need to modify it later you should use a copy
   * instead. Cogl makes copying materials reasonably cheap
   */
//  copy = cogl_material_copy (material);

  sync_surface_geometry();
  progress = clutter_timeline_get_progress (timeline);

  /* Create the constant color to be used when combining the two
   * material layers; we use a black color with an alpha component
   * depending on the current progress of the timeline
   */
//  cogl_color_init_from_4ub (&constant, 0x00, 0x00, 0x00, 0xff * progress);

  /* This sets the value of the constant color we use when combining
   * the two layers
   */
//  cogl_material_set_layer_combine_constant (copy, 1, &constant);

  /* The Texture now owns the material */
//  clutter_texture_set_cogl_material (texture, copy);
//  cogl_handle_unref (copy);


	for(actor_data = actor_data_root; actor_data != NULL && actor_data->actor != NULL; actor_data = actor_data->next){
		buffer = actor_data->buffer;
		w = actor_data->w;
		h = actor_data->h;
		
		if(actor_data->actor != desktop)
			clutter_actor_queue_redraw (actor_data->actor);
	}
}




/* Release the gesture once on geometry changes, focus loss or stage destruction. */
static void
cancel_inferno_pointer(void)
{
	IoaPointerEvent output;

	if(ioa_pointer_event(&inferno_pointer, IoaTouchCancel, 0, 0, 0, Xsize, Ysize, &output))
		xmouse_btn(output.x, output.y, output.buttons);
}


/* Keep logical buffers immutable; move/scale the shared canvas into the current visible area. */
static void
sync_surface_geometry(void)
{
	IoaViewport next;
	int width, height;
	ARect content;

	if(stage == NULL || inferno_canvas == NULL || aapp == NULL || aapp->window == NULL){
		cancel_inferno_pointer();
		inferno_viewport.scale = 0;
		return;
	}
	width = ANativeWindow_getWidth(aapp->window);
	height = ANativeWindow_getHeight(aapp->window);
	content = aapp->contentRect;
	if(!ioa_viewport_fit(&next, Xsize, Ysize, width, height,
			content.left, content.top, content.right, content.bottom)){
		cancel_inferno_pointer();
		inferno_viewport.scale = 0;
		clutter_actor_hide(inferno_canvas);
		return;
	}
	if(next.x != inferno_viewport.x || next.y != inferno_viewport.y || next.scale != inferno_viewport.scale)
		cancel_inferno_pointer();
	inferno_viewport = next;
	clutter_actor_set_position(inferno_canvas, next.x, next.y);
	clutter_actor_set_scale(inferno_canvas, next.scale, next.scale);
	clutter_actor_show(inferno_canvas);
}


/* The linker wrapper runs after amain parses -g and before Inferno starts its workers. */
extern void __real_libinit(void *imod);


/* Allocate the final, word-aligned desktop before attachscreen can publish its buffer. */
void
__wrap_libinit(void *imod)
{
	size_t bytes;

	Xsize &= ~3;
	if(inferno_canvas == NULL || xscreendata != NULL || !ioa_surface_bytes(Xsize, Ysize, &bytes)){
		LOGE("Invalid or repeated Inferno screen initialization");
		return;
	}
	xscreendata = calloc(1, bytes);
	if(xscreendata == NULL){
		LOGE("Cannot allocate Inferno logical screen");
		return;
	}
	clutter_actor_set_size(inferno_canvas, Xsize, Ysize);
	clutter_actor_set_clip(inferno_canvas, 0, 0, Xsize, Ysize);
	sync_surface_geometry();
	__real_libinit(imod);
}


/* Deactivation cannot leave a logical mouse button held after Android hides its window. */
static void
on_stage_deactivate(ClutterActor *actor, gpointer user_data)
{
	(void)actor;
	(void)user_data;
	cancel_inferno_pointer();
}


/* Stop geometry polling before destruction invalidates the stage and its canvas. */
static void
on_stage_destroy(ClutterActor *actor, gpointer user_data)
{
	on_stage_deactivate(actor, user_data);
	ioa_ui_stop();
	if(desktop_timeline != NULL)
		clutter_timeline_stop(desktop_timeline);
	inferno_viewport.scale = 0;
	inferno_canvas = NULL;
	stage = NULL;
}


/* Capture once at the stage, then invert the actual GLES actor transform into Inferno space. */
static gboolean
on_touch(ClutterActor *actor, ClutterEvent *event, gpointer user_data)
{
	IoaPointerEvent output;
	ClutterEventType event_type;
	gfloat x, y, logical_x, logical_y;
	int type;

	(void)actor;
	(void)user_data;
	event_type = clutter_event_type(event);
	switch(event_type){
	case CLUTTER_TOUCH_BEGIN: type = IoaTouchBegin; break;
	case CLUTTER_TOUCH_UPDATE: type = IoaTouchUpdate; break;
	case CLUTTER_TOUCH_END: type = IoaTouchEnd; break;
	case CLUTTER_TOUCH_CANCEL:
		cancel_inferno_pointer();
		return CLUTTER_EVENT_STOP;
	default:
		return CLUTTER_EVENT_PROPAGATE;
	}
	sync_surface_geometry();
	if(inferno_canvas == NULL || aapp == NULL || aapp->window == NULL || inferno_viewport.scale <= 0)
		return CLUTTER_EVENT_STOP;
	clutter_event_get_coords(event, &x, &y);
	if(inferno_viewport.scale <= 0){
		if(type == IoaTouchEnd)
			cancel_inferno_pointer();
		return CLUTTER_EVENT_STOP;
	}
	logical_x = (x-inferno_viewport.x)/inferno_viewport.scale;
	logical_y = (y-inferno_viewport.y)/inferno_viewport.scale;
	if(ioa_pointer_event(&inferno_pointer, type, (uintptr_t)clutter_event_get_event_sequence(event),
			logical_x, logical_y, Xsize, Ysize, &output))
		xmouse_btn(output.x, output.y, output.buttons);
	return CLUTTER_EVENT_STOP;
}


static gboolean
on_button_press (ClutterActor *actor,
          ClutterEvent *event)
{
	ClutterButtonEvent *be = (ClutterButtonEvent*)event;
LOGI("%s: %d e_type=%d, x=%f, y=%f", __func__, __LINE__, clutter_event_type(event), be->x, be->y );

/*
gboolean
clutter_actor_transform_stage_point (ClutterActor *self,
                                     gfloat x,
                                     gfloat y,
                                     gfloat *x_out,
                                     gfloat *y_out);
*/
  return FALSE;
}

static gboolean
on_button_release (ClutterActor *actor,
          ClutterEvent *event)
{
	ClutterButtonEvent *be = (ClutterButtonEvent*)event;
LOGI("%s: %d e_type=%f, x=%d, y=%f", __func__, __LINE__, clutter_event_type(event), be->x, be->y );

  return FALSE;
}





#if 0
static gboolean
on_enter (ClutterActor *actor,
          ClutterEvent *event)
{
  ClutterTransition *t;

  return CLUTTER_EVENT_STOP;

  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.25);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));

  return CLUTTER_EVENT_STOP;
}


static gboolean
on_leave (ClutterActor *actor,
          ClutterEvent *event)
{
  ClutterTransition *t;

  return CLUTTER_EVENT_STOP;

  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.25);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));

  return CLUTTER_EVENT_STOP;
}


static void
on_drag_begin (ClutterDragAction   *action,
               ClutterActor        *actor,
               gfloat               event_x,
               gfloat               event_y,
               ClutterModifierType  modifiers)
{
  gboolean is_copy = (modifiers & CLUTTER_SHIFT_MASK) ? TRUE : FALSE;
  ClutterActor *drag_handle = NULL;
  ClutterTransition *t;

  if (is_copy)
    {
      ClutterActor *stage = clutter_actor_get_stage (actor);

      drag_handle = clutter_actor_new ();
      clutter_actor_set_size (drag_handle, 48, 48);

      clutter_actor_set_background_color (drag_handle, CLUTTER_COLOR_DarkSkyBlue);

      clutter_actor_add_child (stage, drag_handle);
      clutter_actor_set_position (drag_handle, event_x, event_y);
    }
  else
    drag_handle = actor;

  clutter_drag_action_set_drag_handle (action, drag_handle);

#if 0
  /* fully desaturate the actor */
  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.factor");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 1.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));
#endif
  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.25);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));

}


static void
on_drag_end (ClutterDragAction   *action,
             ClutterActor        *actor,
             gfloat               event_x,
             gfloat               event_y,
             ClutterModifierType  modifiers)
{
  ClutterActor *drag_handle;
  ClutterTransition *t;

  drag_handle = clutter_drag_action_get_drag_handle (action);
  if (actor != drag_handle)
    {
      gfloat real_x, real_y;
      ClutterActor *parent;

      /* if we are dragging a copy we can destroy the copy now
       * and animate the real actor to the drop coordinates,
       * transformed in the parent's coordinate space
       */
      clutter_actor_save_easing_state (drag_handle);
      clutter_actor_set_easing_mode (drag_handle, CLUTTER_LINEAR);
      clutter_actor_set_opacity (drag_handle, 0);
      clutter_actor_restore_easing_state (drag_handle);
      g_signal_connect (drag_handle, "transitions-completed",
                        G_CALLBACK (clutter_actor_destroy),
                        NULL);

      parent = clutter_actor_get_parent (actor);
      clutter_actor_transform_stage_point (parent, event_x, event_y,
                                           &real_x,
                                           &real_y);

      clutter_actor_save_easing_state (actor);
      clutter_actor_set_easing_mode (actor, CLUTTER_EASE_OUT_CUBIC);
      clutter_actor_set_position (actor, real_x, real_y);
      clutter_actor_restore_easing_state (actor);
    }

#if 0
  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.factor");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 1.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));
#endif
  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.25);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));
}
#endif



static void
on_paint (ClutterActor *actor, gpointer user_data)
{
//	ClutterActor *source;
//	ClutterActorBox box;
//	CoglHandle material;
//	gfloat width, height;
	guint8 opacity;
  
	ActorData *actor_data = (ActorData*)user_data;
	char *buffer;
	int w, h;

	if(del_flag || actor_data == NULL || actor_data->buffer == NULL || actor_data->actor == NULL)
		return;
	buffer = actor_data->buffer;
  
//	CoglColor color_1, color_2;
//	CoglTextureVertex vertices[4];

	w = actor_data->w;
	h = actor_data->h;

#if 1
	if(actor_data->is_changed){
		clutter_texture_set_from_rgb_data (
		//clutter_texture_set_area_from_rgb_data (
									CLUTTER_TEXTURE (actor),
                                     buffer,
                                     TRUE,
                                     w, h,
                                     w * 4,
                                     4,
                                     CLUTTER_TEXTURE_NONE,
                                     NULL);

		actor_data->is_changed = 0;
	}
#else
	{
		GError *error = NULL;
		ClutterContent	*image=clutter_image_new ();
		clutter_image_set_data (CLUTTER_IMAGE (image),
	                          buffer,
	                          COGL_PIXEL_FORMAT_RGBA_8888,
	                          w, h, 0, &error);
		clutter_actor_set_content (actor, image);
	}
#endif
//	clutter_actor_set_position(actor, actor_data->x, actor_data->y);

	opacity = 180;
	if(topmost == actor)
		opacity = 200;
	clutter_actor_set_opacity (actor, opacity);

#if 0
// VVV
  /* if we don't have a source actor, don't paint */
  source = clutter_clone_get_source (CLUTTER_CLONE (actor));
  if (source == NULL)
  	return;
//    goto out;

  /* if the source texture does not have any content, don't paint */
  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (actor));
  if (material == NULL)
  	return;
//    goto out;

//LOGI("%s: %d actor=%x, buf=%x", __func__, __LINE__, actor, buffer);
  /* get the size of the reflection */
//  clutter_actor_get_allocation_box (actor, &box);
//  clutter_actor_box_get_size (&box, &width, &height);
//  w = (int)width;
//  h = (int)height;
width = w;
height = h;

//LOGI("%s: %d w=%d, h=%d", __func__, __LINE__, w, h);
  /* get the composite opacity of the actor */
  opacity = clutter_actor_get_opacity (actor);

  /* figure out the two colors for the reflection: the first is
   * full color and the second is the same, but at 0 opacity
   */
//LOGI("%s: %d", __func__, __LINE__);
  cogl_color_init_from_4f (&color_1, 1.0, 1.0, 1.0, opacity / 255.);
  cogl_color_premultiply (&color_1);
  cogl_color_init_from_4f (&color_2, 1.0, 1.0, 1.0, 0.0);
  cogl_color_premultiply (&color_2);

//LOGI("%s: %d", __func__, __LINE__);
  /* now describe the four vertices of the quad; since it has
   * to be a reflection, we need to invert it as well
   */
  vertices[0].x = actor_data->x; vertices[0].y = actor_data->y+h; vertices[0].z = 0;
  vertices[0].tx = 0.0; vertices[0].ty = 1.0;
  vertices[0].color = color_1;

  vertices[1].x = actor_data->x+width; vertices[1].y = actor_data->y+h; vertices[1].z = 0;
  vertices[1].tx = 1.0; vertices[1].ty = 1.0;
  vertices[1].color = color_1;

  vertices[2].x = actor_data->x+width; vertices[2].y = actor_data->y+h+height; vertices[2].z = 0;
  vertices[2].tx = 1.0; vertices[2].ty = 0.0;
  vertices[2].color = color_2;

  vertices[3].x = actor_data->x; vertices[3].y = actor_data->y+h+height; vertices[3].z = 0;
  vertices[3].tx = 0.0; vertices[3].ty = 0.0;
  vertices[3].color = color_2;

  /* paint the same texture but with a different geometry */
  cogl_set_source (material);
  cogl_polygon (vertices, 4, TRUE);
// AAA
#endif

//LOGI("%s: %d", __func__, __LINE__);
//	cogl_set_source(
//		cogl_texture_new_from_buffer(xscreendata, Xsize, Ysize, 4)
//	);


//LOGI("%s: %d", __func__, __LINE__);
//out:
  /* prevent the default clone handler from running */
//  g_signal_stop_emission_by_name (actor, "paint");
}

#if 0
static void
my_stage_paint_node (ClutterActor     *actor,
                     ClutterPaintNode *root)
{
  ClutterPaintNode *node;
  ClutterActorBox box;
  
  CoglTexture* cogl_texture;
  
  if(xscreendata == NULL) return;

  //&ast; where the content of the actor should be painted &ast;/
  clutter_actor_get_allocation_box (actor, &box);

  cogl_texture = cogl_texture_new_from_buffer(xscreendata, Xsize, Ysize, 4);
  
  //&ast; the cogl_texture variable is set elsewhere &ast;/
  node = clutter_texture_node_new (cogl_texture, CLUTTER_COLOR_White,
                                   CLUTTER_SCALING_FILTER_TRILINEAR,
                                   CLUTTER_SCALING_FILTER_LINEAR);

  //&ast; paint the content of the node using the allocation &ast;/
  clutter_paint_node_add_rectangle (node, &box);

  //&ast; add the node, and transfer ownership &ast;/
  clutter_paint_node_add_child (root, node);
  clutter_paint_node_unref (node);
}
#endif



static gboolean
toggle_fullscreen (ClutterActor *button,
                   ClutterStage *stage)
{
  clutter_stage_set_fullscreen (stage, !clutter_stage_get_fullscreen (stage));

  return TRUE;
}

static gboolean
toggle_keyboard (ClutterActor *button,
                 ClutterAndroidApplication *application)
{
  static gboolean visible = FALSE;

  visible = !visible;
#ifndef _PC_VERSION
  clutter_android_application_show_keyboard (application, visible, FALSE);
#endif

  return TRUE;
}

#if 0
static gboolean
_rotate_gesture_begin (ClutterAction *action,
                       ClutterActor *actor,
                       TestData *data)
{
  clutter_actor_remove_action (actor, data->drag_action);

  return TRUE;
}

static gboolean
_rotate_gesture_end (ClutterAction *action,
                     ClutterActor *actor,
                     TestData *data)
{
  data->drag_action = clutter_drag_action_new ();
  clutter_actor_add_action (actor, data->drag_action);

  return TRUE;
}
#endif


int is_shaders_inited = 0;
static char* default_win_shader = NULL;


ClutterActor*
ioa_render_attach_clutter_actor(char* buffer, int min_x, int min_y, int max_x, int max_y){
	GError *error = NULL;
	ClutterActor *actor;
	ClutterTimeline *timeline = NULL;
	ActorData *actor_data = NULL;
	char *buffer_int;
	int w, h;
	
	w = max_x - min_x;
	h = max_y - min_y;

	if(buffer == NULL || w <= 1 || h <= 1)
		return NULL;
	
	size_t allocation;
	if(!ioa_surface_bytes(w, h, &allocation))
		return NULL;
	buffer_int = calloc(1, allocation);
//LOGI("%s: %d buf=%x, w=%d, h=%d", __func__, __LINE__, buffer_int, w, h);

	if(buffer_int == NULL)
		return NULL;
	
	actor = clutter_texture_new ();
	clutter_actor_set_position(actor, min_x, min_y);
	clutter_actor_set_size(actor, w, h);
	clutter_actor_set_background_color (actor, CLUTTER_COLOR_Orange);
//LOGI("%s: %d actor=%x", __func__, __LINE__, actor);

	actor_data = malloc(sizeof(ActorData));
    if(actor_data == NULL){
        free(buffer_int);
        clutter_actor_destroy(actor);
        return NULL;
    }
	memset(actor_data, 0, sizeof(ActorData));
//LOGI("%s: %d actor_data=%x", __func__, __LINE__, actor_data);
	actor_data->actor = CLUTTER_ACTOR(actor);
	actor_data->buffer_ext = buffer;
	actor_data->buffer = buffer_int;
	actor_data->w = w;
	actor_data->h = h;
	actor_data->x = min_x;
	actor_data->y = min_y;
	actor_data->is_changed = 0;
	
	actor_data->next_in_layer = NULL;
	actor_data->layer = NULL;
	actor_data->layer_id = 0;
	
	actor_data->next = actor_data_root;


	if(!is_shaders_inited){
		FILE *f;
		char s[128];
		int sz;
		
		snprintf(s, 127, "%s/lib/shaders/win_default.glsl", rootdir);
		f = fopen(s, "r");
		if(f){
			fseek(f, 0L, SEEK_END);
			sz = ftell(f);
			fseek(f, 0L, SEEK_SET);
            if(sz < 0 || sz > 1024*1024){
                fclose(f);
                f = NULL;
            }else
                default_win_shader = malloc((size_t)sz + 1);
            if(default_win_shader != NULL){
                size_t received = fread(default_win_shader, 1, (size_t)sz, f);
                default_win_shader[received] = 0;
            }
            if(f != NULL)
                fclose(f);
		}
		is_shaders_inited = 1;
	}
				

	/**/
	if(default_win_shader && actor_data_root){
		ClutterEffect *effect = clutter_shader_effect_new (CLUTTER_FRAGMENT_SHADER);
		clutter_shader_effect_set_shader_source (
									CLUTTER_SHADER_EFFECT (effect), 
									default_win_shader);
		clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect), "tex",
								   G_TYPE_INT, 1, 0);
		clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect), "x0",
								   G_TYPE_FLOAT, 1, (float)min_x);
		clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect), "y0",
								   G_TYPE_FLOAT, 1, (float)min_y);
		clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect), "width",
								   G_TYPE_FLOAT, 1, (float)w);
		clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (effect), "height",
								   G_TYPE_FLOAT, 1, (float)h);	
		clutter_actor_add_effect (actor, effect);	
	//	g_object_unref(effect);
		actor_data->effect = effect;
	}else
		actor_data->effect = NULL;
	/**/

	if(!actor_data_root){
//LOGI("%s: %d", __func__, __LINE__);

//LOGI("%s: %d", __func__, __LINE__);
		clutter_actor_set_reactive (actor, TRUE);
//	}else{
		desktop = actor;
		g_signal_connect (desktop_timeline, "new-frame", G_CALLBACK (_update_progress_cb), desktop);
		clutter_timeline_start(desktop_timeline);
		clutter_actor_add_child (inferno_canvas, actor);
	}else{
		clutter_actor_add_child (inferno_canvas, actor);
	}
	g_signal_connect (actor, "paint", G_CALLBACK (on_paint), actor_data);
//	clutter_actor_add_child (inferno_canvas, actor);

//	clutter_actor_raise_top(actor);
//	topmost = actor;

//LOGI("%s: %d", __func__, __LINE__);
	actor_data_root = actor_data;
	
	return actor;
}



void
ioa_render_clutter_ext_win_add_layer(ClutterActor *actor, int layerid){
	ActorData *actor_data = find_ActorData(actor);
	ActorLayer *layer;

LOGI("%s: %d actor=%x, layer_id=%x", __func__, __LINE__, actor, layerid);

	if(!actor_data)
		return;

	layer = malloc(sizeof(ActorLayer));
	memset(layer, 0, sizeof(ActorLayer));

	layer->layer_id = layerid;
	layer->main_actor = actor;
	layer->main_actor_data = actor_data;
	layer->first_child = NULL;
	
	layer->next = layer_root;
	layer_root = layer;

LOGI("%s: %d layer=%x", __func__, __LINE__, layer);
	if(actor_data){
		actor_data->next_in_layer = NULL;
		actor_data->layer_id = layerid;
		actor_data->layer = layer;
	}

	
	clutter_actor_raise_top(actor);
	topmost = actor;
	
}


void
ioa_render_clutter_ext_win_child_to_layer(ClutterActor *actor, int layerid){
	ActorData *actor_data = find_ActorData(actor);
	ActorData *main_actor_data;
	ActorLayer *layer;

	if(!actor_data)
		return;

	for(layer = layer_root; layer != NULL; layer = layer->next){
		if(layer->layer_id == layerid)
			break;
	}

	if(!layer)
		return;
LOGI("%s: %d layer=%x", __func__, __LINE__, layer);

	main_actor_data = layer->main_actor_data;
	if(!main_actor_data || main_actor_data->actor == desktop)
		return;

LOGI("%s: %d main_actor_data=%x", __func__, __LINE__, main_actor_data);
	actor_data->next_in_layer = main_actor_data->next_in_layer;
	main_actor_data->next_in_layer = actor_data;

	layer->first_child = actor_data;
	
	actor_data->layer_id = layerid;
	actor_data->layer = layer;

//	actor_data->x += main_actor_data->x;
//	actor_data->y += main_actor_data->y;

//	clutter_actor_remove_child (/*desktop*/stage, actor);
//	clutter_actor_add_child (main_actor_data->actor, actor);

//	clutter_actor_set_position(actor, 
//		actor_data->x - main_actor_data->x, 
//		actor_data->y - main_actor_data->y
//	);

	if(main_actor_data->actor != actor)
		clutter_actor_set_position(actor, 
			actor_data->x + main_actor_data->x, 
			actor_data->y + main_actor_data->y
		);

//	clutter_actor_set_position(actor, 
//		actor_data->x, 
//		actor_data->y
//	);

}



void 
ioa_render_clutter_move_actor(ClutterActor *actor, int x, int y){
	ActorData *actor_data = find_ActorData(actor);
	ClutterEffect *effect;
	
	ActorLayer *layer;
	ActorData *main_actor_data, *tmp_actor_data;
	
	if(actor_data == NULL)
		return;
	
	main_actor_data = NULL;
	
	layer = actor_data->layer;
LOGI("%s: %d layer=%x", __func__, __LINE__, layer);
	if(layer){
		main_actor_data = layer->main_actor_data;
LOGI("%s: %d main_actor_data=%x", __func__, __LINE__, main_actor_data);
		if(main_actor_data && main_actor_data->actor != desktop){
LOGI("%s: %d main_actor_data->actor=%x, actor=%x, stage=%x, desktop=%x", __func__, __LINE__, main_actor_data->actor, actor, stage, desktop);
			if(main_actor_data->actor == actor){
			
LOGI("%s: %d main_actor_data->actor=%x", __func__, __LINE__, main_actor_data->actor);
			for(
				tmp_actor_data = main_actor_data->next_in_layer; 
				tmp_actor_data != NULL; 
				tmp_actor_data = tmp_actor_data->next_in_layer
			){
				int dx, dy, xc, yc;

				dx = (x - actor_data->x);
				dy = (y - actor_data->y);

				xc = x + tmp_actor_data->x;
				yc = y + tmp_actor_data->y;

				//tmp_actor_data->x += dx;
				//tmp_actor_data->y += dy;
				
LOGI("%s: %d tmp_actor_data=%x, x=%d, y=%d", __func__, __LINE__, tmp_actor_data, xc, yc);
				if(tmp_actor_data->effect){
					clutter_shader_effect_set_uniform (
											CLUTTER_SHADER_EFFECT (tmp_actor_data->effect), 
											"x0",
											G_TYPE_FLOAT, 1, (float)xc);
					clutter_shader_effect_set_uniform (
											CLUTTER_SHADER_EFFECT (tmp_actor_data->effect), 
											"y0",
											G_TYPE_FLOAT, 1, (float)yc);
				}
				
				clutter_actor_set_position(tmp_actor_data->actor, xc, yc);
			}

			//return;

			}else{
				int dx, dy, xc, yc;

				dx = (x - main_actor_data->x);
				dy = (y - main_actor_data->y);

				xc = dx + actor_data->x;
				yc = dy + actor_data->y;

				//tmp_actor_data->x += dx;
				//tmp_actor_data->y += dy;
					
LOGI("%s: %d actor_data=%x, x=%d, y=%d", __func__, __LINE__, actor_data, xc, yc);
				if(actor_data->effect){
					clutter_shader_effect_set_uniform (
										CLUTTER_SHADER_EFFECT (actor_data->effect), 
										"x0",
										G_TYPE_FLOAT, 1, (float)xc);
					clutter_shader_effect_set_uniform (
										CLUTTER_SHADER_EFFECT (actor_data->effect), 
										"y0",
										G_TYPE_FLOAT, 1, (float)yc);
				}
				
				clutter_actor_set_position(actor_data->actor, xc, yc);

				return;
			}
		}
	}


	effect = actor_data->effect;

	actor_data->x = x;
	actor_data->y = y;

/*
  if (aapp != NULL && aapp->window != NULL){
  	int x, y;
	
	x = ANativeWindow_getWidth (aapp->window);
	y = ANativeWindow_getHeight (aapp->window);
	//clutter_actor_get_size(stage, &x, &y);
	
		LOGI ("mov @ %d x %d", x, y);
//		clutter_actor_set_size (CLUTTER_ACTOR (stage), Xsize, Ysize);
	
//	clutter_set_default_frame_rate(15);
  }
*/

LOGI("%s: %d actor=%x, x=%d, y=%d", __func__, __LINE__, actor, x, y);

	if(effect){
		clutter_shader_effect_set_uniform (
								CLUTTER_SHADER_EFFECT (effect), "x0",
								G_TYPE_FLOAT, 1, (float)x);
		clutter_shader_effect_set_uniform (
								CLUTTER_SHADER_EFFECT (effect), "y0",
								G_TYPE_FLOAT, 1, (float)y);
	}

	clutter_actor_set_position(actor, x, y);
}


void
ioa_render_clutter_clipr_actor(ClutterActor *actor, int min_x, int min_y, int max_x, int max_y){
	ActorData *actor_data = find_ActorData(actor);
	int w, h, i, j;
	unsigned int *po, *pn;
	char *buf2;
	
	if(actor_data == NULL)
		return;

	w = (max_x-min_x); //+(min_x-actor_data->x);
	h = (max_y-min_y); //+(min_y-actor_data->y);
	
//	if(actor_data->w < w || actor_data->h < h){
	if(min_x == 0 || min_y == 0){
LOGI("%s: %d actor=%x, old_buf=%x, wh=(%d, %d)", __func__, __LINE__, actor, actor_data->buffer, w, h);

		buf2 = malloc((w+1)*(h+1)*4);

		if(actor_data->buffer){
			pn = buf2;
			po = actor_data->buffer;
		
/**
			for(i=0; i < actor_data->h && i < h; i++){
				for(j=0; j < actor_data->w && j < w; j++){
					pn[j] = po[j];
				}
				pn += w;
				po += actor_data->w;
			}
/**/
			free(actor_data->buffer);
//LOGI("%s: %d", __func__, __LINE__);
//			actor_data->buffer = realloc(actor_data->buffer, (w+1)*(h+1)*4);
//LOGI("%s: %d", __func__, __LINE__);
		}
		actor_data->buffer = buf2;
		
		actor_data->w = w;
		actor_data->h = h;

//		actor_data->x = min_x;
//		actor_data->y = min_y;
		
//		clutter_actor_set_position(actor, min_x, min_y);
		clutter_actor_set_size(actor, w, h);
	}
//LOGI("%s: %d new_buf=%x", __func__, __LINE__, actor_data->buffer);
}


#if 0
int
clutter_ext_win_draw_1byte(ClutterActor *actor, int dst_x, int dst_y, ulong v, int w, int h, int swid){
	ActorData *actor_data = find_ActorData(actor);
	char *dp;
	int dwid;
	int y, x, xd, yd;
	int dx, dy;
	ulong *lptr;
	
	if(actor_data == NULL)
		return 0;

	dx = dst_x;// - actor_data->x;
	dy = dst_y;// - actor_data->y;

	dwid = actor_data->w * 4;
	dp = actor_data->buffer + dy*dwid + dx*4;

LOGI("%s: %d", __func__, __LINE__);
	for(y = 0, yd=0; y < h; y++){
		lptr = dp;
		if(/*y >= actor_data->y &&*/ y < /*actor_data->y +*/ actor_data->h - dy){
			for(x=0, xd=0; x<w; x++){
				if(
					/*x >= actor_data->x &&*/
					x < /*actor_data->x +*/ actor_data->w - dx
				){
					*lptr++ = v;
//					dp[xd*4 + 0] = sp[x*4 + 2];
//					dp[xd*4 + 1] = sp[x*4 + 1];
//					dp[xd*4 + 2] = sp[x*4 + 0];
//					dp[xd*4 + 3] = 0xff;
//					xd++;
				}
				//memmove(dp, sp, nb);
			}
//			yd++;
			dp += dwid;
		}
	}
LOGI("%s: %d", __func__, __LINE__);
	return 1;
}
#endif

void
ioa_render_clutter_ext_win_free(ClutterActor *actor){
	ActorData *act_dat_ptr, **pactor_data, *actor_data = find_ActorData(actor);
	char *buffer;

	ActorLayer *layer;
	ActorData *main_actor_data, *tmp_actor_data, **ptmp_actor_data;
	
	if(actor_data == NULL)
		return;

	main_actor_data = NULL;
		
	layer = actor_data->layer;
LOGI("%s: %d layer=%x", __func__, __LINE__, layer);
	if(layer){
		main_actor_data = layer->main_actor_data;
LOGI("%s: %d main_actor_data=%x", __func__, __LINE__, main_actor_data);
		if(main_actor_data){
LOGI("%s: %d main_actor_data->actor=%x", __func__, __LINE__, main_actor_data->actor);

			ptmp_actor_data = &main_actor_data->next_in_layer;
			for(
				tmp_actor_data = main_actor_data->next_in_layer; 
				tmp_actor_data != NULL; 
				tmp_actor_data = tmp_actor_data->next_in_layer
			){
LOGI("%s: %d tmp_actor_data=%x", __func__, __LINE__, tmp_actor_data);
				if(tmp_actor_data->actor == actor){
					*ptmp_actor_data = tmp_actor_data->next_in_layer;
					break;
				}
				ptmp_actor_data = &tmp_actor_data->next_in_layer;
			}
		}
	}

	if(tmp_actor_data){
//		free(tmp_actor_data);
	}

LOGI("%s: %d", __func__, __LINE__);
	for(
		pactor_data = &actor_data_root, act_dat_ptr = actor_data_root; 
		act_dat_ptr != NULL;
		act_dat_ptr = act_dat_ptr->next
	){
		if(act_dat_ptr->actor == actor){
//LOGI("%s: %d", __func__, __LINE__);
			*pactor_data = act_dat_ptr->next;
			actor_data = act_dat_ptr;
//LOGI("%s: %d", __func__, __LINE__);
			break;
		}
		pactor_data = &(act_dat_ptr->next);
//LOGI("%s: %d", __func__, __LINE__);
	}
	if(act_dat_ptr == NULL)
		return;

	del_flag = 1;

	//g_signal_handlers_disconnect_by_data(actor, actor_data);
//LOGI("%s: %d act_dat=0x%X", __func__, __LINE__, actor_data);
	actor_data->actor = NULL;
	clutter_actor_destroy(actor);
	
	if(topmost == actor)
		topmost = NULL;
//LOGI("%s: %d act_buf=0x%X", __func__, __LINE__, actor_data->buffer);

	buffer = actor_data->buffer;
	actor_data->buffer = NULL;

	free(buffer);
//LOGI("%s: %d", __func__, __LINE__);

	if(actor_data->effect)
		g_object_unref(actor_data->effect);

	free(actor_data);
//LOGI("%s: %d", __func__, __LINE__);

	del_flag = 0;
}


void
ioa_render_clutter_ext_win_tofront(ClutterActor *actor){
	ActorData *actor_data = find_ActorData(actor);
	ActorLayer *layer;
	ActorData *main_actor_data, *tmp_actor_data;
	
	if(actor_data == NULL)
		return;

	main_actor_data = NULL;
	
	layer = actor_data->layer;
LOGI("%s: %d layer=%x", __func__, __LINE__, layer);
	if(layer){
		main_actor_data = layer->main_actor_data;
LOGI("%s: %d main_actor_data=%x", __func__, __LINE__, main_actor_data);
		if(main_actor_data){
LOGI("%s: %d main_actor_data->actor=%x", __func__, __LINE__, main_actor_data->actor);
			clutter_actor_raise_top(main_actor_data->actor);
			topmost = main_actor_data->actor;

			for(tmp_actor_data = main_actor_data->next_in_layer; tmp_actor_data != NULL; tmp_actor_data = tmp_actor_data->next_in_layer){
LOGI("%s: %d tmp_actor_data=%x", __func__, __LINE__, tmp_actor_data);
				if(tmp_actor_data->actor && tmp_actor_data->actor != actor)
					clutter_actor_raise_top(tmp_actor_data->actor);
			}
		}
	}
LOGI("%s: %d", __func__, __LINE__);
	if(layer == NULL || main_actor_data == NULL || main_actor_data->actor != actor){
		clutter_actor_raise_top(actor);
		topmost = actor;
LOGI("%s: %d", __func__, __LINE__);
	}

	topmost = actor;
}


void
ioa_render_clutter_ext_win_torear(ClutterActor *actor){
	ActorData *actor_data = find_ActorData(actor);
	if(actor_data == NULL)
		return;
	clutter_actor_lower_bottom(actor);
	
	if(topmost == actor)
		topmost = NULL;
}


void
ioa_render_clutter_ext_win_repaint(ClutterActor *actor){
	GError *error = NULL;
	ActorData *actor_data;// = find_ActorData(actor);
	char *buffer, *buffer_ext;
	int w, h;

	if(del_flag)
		return;

	actor_data = find_ActorData(actor);

//LOGI("%s: %d actor=%x", __func__, __LINE__, actor);
	if(actor_data == NULL)
		return;

	buffer = actor_data->buffer;
	buffer_ext = actor_data->buffer_ext;
	w = actor_data->w;
	h = actor_data->h;

	if(buffer < 0x10000 || buffer_ext < 0x10000)
		return;
//LOGI("%s: %d actor=%x, buf=%x, buf_ext=%x, w=%d, h=%d", 
//	__func__, __LINE__, actor, buffer, buffer_ext, w, h);

#if 1
	{
		int i, j, idx;
		for(j=0; j<h; j++){
			for(i=0; i<w; i++){
				idx = ((j*w)<<2) + (i<<2);
				buffer[idx + 0] = buffer_ext[idx + 2];
				buffer[idx + 1] = buffer_ext[idx + 1];;
				buffer[idx + 2] = buffer_ext[idx + 0];;
				buffer[idx + 3] = 0xff;
			}
		}
	}
	actor_data->is_changed = 1;
	clutter_actor_queue_redraw(actor);
#endif

//LOGI("%s: %d", __func__, __LINE__);
#if 0
	clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (actor),
										buffer,
										TRUE,
										w, h,
										w * 4,
										4,
										CLUTTER_TEXTURE_NONE,
										&error);
#endif
//LOGI("%s: %d", __func__, __LINE__);
}


char*
ioa_render_clutter_ext_win_get_dp(ClutterActor *actor, int dst_x, int dst_y, int *dwid, int *w, int *h){
	ActorData *actor_data = find_ActorData(actor);
	int dx, dy;
	int wid;
	char *dp, *ep;
	
	if(actor_data == NULL || actor_data->buffer_ext == NULL)
		return NULL;
	
	dx = dst_x - actor_data->x;
	dy = dst_y - actor_data->y;
	
	if(dx<0 || dy<0 || dx >= actor_data->w || dy >= actor_data->h)
		return NULL;

	wid = actor_data->w * sizeof(ulong);
	
	dp = actor_data->buffer_ext + dy*wid + dx*sizeof(ulong);
	ep = actor_data->buffer_ext + wid*actor_data->h;
	
//LOGI("%s: %d actor=%x, dst_buf=%x,\n.. actor_xy=(%d, %d), actor_wh=(%d, %d)\n.. dst: xy=(%d, %d)\n.. dxdy=(%d, %d)\n.. dp = %x (%x - %x)", 
//			__func__, __LINE__, 
//			actor, actor_data->buffer_ext, 
//			actor_data->x, actor_data->y,
//			actor_data->w, actor_data->h,
//			dst_x, dst_y,
//			dx, dy,
//			dp, actor_data->buffer_ext, ep
//			);
			
	if(dp < actor_data->buffer_ext || dp >= ep)
		return NULL;
	
	if(dwid)
		*dwid = wid;
	if(w)
		*w = actor_data->w - dx;
	if(h)
		*h = actor_data->h - dy;

	return dp;
}


int
ioa_render_clutter_ext_win_get_coords(ClutterActor *actor, int *x, int *y, int *w, int *h){
	ActorData *actor_data = find_ActorData(actor);
	
	if(actor_data == NULL)
		return 0;

	if(x) *x = actor_data->x;
	if(y) *y = actor_data->y;
	if(w) *w = actor_data->w;
	if(h) *h = actor_data->h;

	return 1;
}


#if 0
int
clutter_ext_win_draw(ClutterActor *actor, int dst_x, int dst_y, char *src_buf, int nb, int h, int swid){
	ActorData *actor_data = find_ActorData(actor);
	char *dp, *sp;
	int dwid;
	int y, x, xd, yd;
	int dx, dy;
	
	if(actor_data == NULL || src_buf == NULL)
		return 0;
		
//	if(swid < nb)
//		return 1;

	{
		return 0;
	}

#if 0
	dx = dst_x;// - actor_data->x;
	dy = dst_y;// - actor_data->y;

LOGI("%s: %d actor=%x, dst_buf=%x, wh=(%d, %d)\n.. dst: xy=(%d, %d),\n.. src: buf=%x, nb=%d, h=%d, swid=%d\n..actor: xy=(%d, %d), dxy=(%d, %d)", 
			__func__, __LINE__, actor, actor_data->buffer, 
			actor_data->w, actor_data->h,
			dst_x, dst_y, 
			src_buf, nb, h, swid, 
			actor_data->x, actor_data->y, dx, dy);

	dwid = actor_data->w * 4;
	
	dp = actor_data->buffer_ext + dy*dwid + dx*4;
	sp = src_buf;
	
LOGI("%s: %d", __func__, __LINE__);
	for(y = 0, yd=0; y < h; y++){
		if(/*y >= actor_data->y &&*/ y < /*actor_data->y +*/ actor_data->h - dy){
			for(x=0, xd=0; x<nb/4; x++){
				if(
					/*x >= actor_data->x &&*/
					x < /*actor_data->x +*/ actor_data->w - dx
				){
					dp[xd*4 + 0] = sp[x*4 + 2];
					dp[xd*4 + 1] = sp[x*4 + 1];
					dp[xd*4 + 2] = sp[x*4 + 0];
					dp[xd*4 + 3] = 0xff;
					xd++;
				}
			}
			yd++;
			dp += dwid;
		}
		sp += swid;
	}	
	actor_data->is_changed = 1;
LOGI("%s: %d", __func__, __LINE__);
	return 1;
#endif
}
#endif


typedef struct IoaRenderCall IoaRenderCall;

struct IoaRenderCall {
    unsigned int kind; // Renderer operation selected by the public wrapper.
    ClutterActor *actor, *created; // Borrowed target and created actor returned synchronously.
    char *buffer, *pixels; // Borrowed VM storage and returned drawing address.
    int x, y, w, h, id; // Logical rectangle or layer identity, never Android touch coordinates.
    int *ox, *oy, *ow, *oh, *stride; // Borrowed synchronous result locations.
    int value; // Boolean result for geometry lookup.
};


/* Execute every native actor operation only on the Clutter context owner. */
static void
ioa_render_dispatch(void *argument)
{
    IoaRenderCall *c = argument;

    switch(c->kind){
    case 0: c->created = ioa_render_attach_clutter_actor(c->buffer,c->x,c->y,c->w,c->h); break; // attach_clutter_actor.
    case 1: ioa_render_clutter_ext_win_add_layer(c->actor,c->id); break; // clutter_ext_win_add_layer.
    case 2: ioa_render_clutter_ext_win_child_to_layer(c->actor,c->id); break; // clutter_ext_win_child_to_layer.
    case 3: ioa_render_clutter_move_actor(c->actor,c->x,c->y); break; // clutter_move_actor.
    case 4: ioa_render_clutter_clipr_actor(c->actor,c->x,c->y,c->w,c->h); break; // clutter_clipr_actor.
    case 5: ioa_render_clutter_ext_win_free(c->actor); break; // clutter_ext_win_free.
    case 6: ioa_render_clutter_ext_win_tofront(c->actor); break; // clutter_ext_win_tofront.
    case 7: ioa_render_clutter_ext_win_torear(c->actor); break; // clutter_ext_win_torear.
    case 8: ioa_render_clutter_ext_win_repaint(c->actor); break; // clutter_ext_win_repaint.
    case 9: c->pixels = ioa_render_clutter_ext_win_get_dp(c->actor,c->x,c->y,c->stride,c->ow,c->oh); break; // clutter_ext_win_get_dp.
    case 10: c->value = ioa_render_clutter_ext_win_get_coords(c->actor,c->ox,c->oy,c->ow,c->oh); break; // clutter_ext_win_get_coords.
    }
}


/* Marshal attach_clutter_actor while its caller keeps native storage alive. */
ClutterActor*
attach_clutter_actor(char *buffer, int min_x, int min_y, int max_x, int max_y)
{
    IoaRenderCall c = {0};

    c.kind=0; c.buffer=buffer; c.x=min_x; c.y=min_y; c.w=max_x; c.h=max_y;
    if(!ioa_ui_call(ioa_render_dispatch, &c))
        return NULL;
    return c.created;
}


/* Marshal clutter_ext_win_add_layer while its caller keeps native storage alive. */
void
clutter_ext_win_add_layer(ClutterActor *actor, int layerid)
{
    IoaRenderCall c = {0};

    c.kind=1; c.actor=actor; c.id=layerid;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_child_to_layer while its caller keeps native storage alive. */
void
clutter_ext_win_child_to_layer(ClutterActor *actor, int layerid)
{
    IoaRenderCall c = {0};

    c.kind=2; c.actor=actor; c.id=layerid;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_move_actor while its caller keeps native storage alive. */
void
clutter_move_actor(ClutterActor *actor, int x, int y)
{
    IoaRenderCall c = {0};

    c.kind=3; c.actor=actor; c.x=x; c.y=y;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_clipr_actor while its caller keeps native storage alive. */
void
clutter_clipr_actor(ClutterActor *actor, int min_x, int min_y, int max_x, int max_y)
{
    IoaRenderCall c = {0};

    c.kind=4; c.actor=actor; c.x=min_x; c.y=min_y; c.w=max_x; c.h=max_y;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_free while its caller keeps native storage alive. */
void
clutter_ext_win_free(ClutterActor *actor)
{
    IoaRenderCall c = {0};

    c.kind=5; c.actor=actor;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_tofront while its caller keeps native storage alive. */
void
clutter_ext_win_tofront(ClutterActor *actor)
{
    IoaRenderCall c = {0};

    c.kind=6; c.actor=actor;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_torear while its caller keeps native storage alive. */
void
clutter_ext_win_torear(ClutterActor *actor)
{
    IoaRenderCall c = {0};

    c.kind=7; c.actor=actor;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_repaint while its caller keeps native storage alive. */
void
clutter_ext_win_repaint(ClutterActor *actor)
{
    IoaRenderCall c = {0};

    c.kind=8; c.actor=actor;
    ioa_ui_call(ioa_render_dispatch, &c);
}


/* Marshal clutter_ext_win_get_dp while its caller keeps native storage alive. */
char*
clutter_ext_win_get_dp(ClutterActor *actor, int dst_x, int dst_y, int *dwid, int *w, int *h)
{
    IoaRenderCall c = {0};

    if(dwid != NULL) *dwid = 0;
    if(w != NULL) *w = 0;
    if(h != NULL) *h = 0;
    c.kind=9; c.actor=actor; c.x=dst_x; c.y=dst_y; c.stride=dwid; c.ow=w; c.oh=h;
    if(!ioa_ui_call(ioa_render_dispatch, &c))
        return NULL;
    return c.pixels;
}


/* Marshal clutter_ext_win_get_coords while its caller keeps native storage alive. */
int
clutter_ext_win_get_coords(ClutterActor *actor, int *x, int *y, int *w, int *h)
{
    IoaRenderCall c = {0};

    if(x != NULL) *x = 0;
    if(y != NULL) *y = 0;
    if(w != NULL) *w = 0;
    if(h != NULL) *h = 0;
    c.kind=10; c.actor=actor; c.ox=x; c.oy=y; c.ow=w; c.oh=h;
    if(!ioa_ui_call(ioa_render_dispatch, &c))
        return 0;
    return c.value;
}


/* Prepare native storage and the GLES canvas before parsing Inferno startup options. */
static gboolean
start_inferno_stage (ClutterAndroidApplication *application,
           TestData*                  data)
{
  ClutterActor *square, *hand, *text, *clone;
  ClutterConstraint *constraint;
  ClutterVertex vertex;
  ClutterActor *box;
  
  ActorData *actor_data = NULL;
  
  ClutterAction *action;

  GError *error = NULL;
  IoaViewport initial_view;

  /*struct android_app* */aapp = application->android_application;

LOGI("%s: %d aapp=%x", __func__, __LINE__, aapp);

//	ClutterContent *image;

	actor_data_root = NULL;

  stage = clutter_stage_get_default();
  clutter_actor_set_background_color(stage, CLUTTER_COLOR_Black);
  if(aapp == NULL || aapp->window == NULL || aapp->activity == NULL)
      return FALSE;
  Xsize = ANativeWindow_getWidth(aapp->window);
  Ysize = ANativeWindow_getHeight(aapp->window);
  if(!ioa_viewport_fit(&initial_view, Xsize, Ysize, Xsize, Ysize,
          aapp->contentRect.left, aapp->contentRect.top, aapp->contentRect.right, aapp->contentRect.bottom))
      return FALSE;
  /* The initial logical desktop occupies the available content rectangle, not hidden system bars. */
  Xsize = initial_view.available_width;
  Ysize = initial_view.available_height;
  clutter_actor_set_size(stage, ANativeWindow_getWidth(aapp->window), ANativeWindow_getHeight(aapp->window));
  inferno_canvas = clutter_actor_new();
  clutter_actor_set_size(inferno_canvas, Xsize, Ysize);
  clutter_actor_set_clip(inferno_canvas, 0, 0, Xsize, Ysize);
  clutter_actor_add_child(stage, inferno_canvas);
  clutter_actor_set_reactive(stage, TRUE);
  g_signal_connect(stage, "captured-event", G_CALLBACK(on_touch), NULL);
  g_signal_connect(stage, "deactivate", G_CALLBACK(on_stage_deactivate), NULL);
  g_signal_connect(stage, "destroy", G_CALLBACK(on_stage_destroy), NULL);
  sync_surface_geometry();

//#ifndef _PC_VERSION
//  clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
//#endif




//  Xsize = clutter_actor_get_width (stage);
//  Ysize = clutter_actor_get_height (stage);
  
  
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

		
	desktop_timeline = clutter_timeline_new(1000);
	clutter_timeline_set_repeat_count(desktop_timeline, -1);
	clutter_timeline_set_auto_reverse(desktop_timeline, TRUE);
	
#if 0
	desktop = clutter_texture_new ();
/**
	clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (desktop),
                                     xscreendata,
                                     TRUE,
                                     Xsize, Ysize,
                                     Xsize * 4,
                                     4,
                                     CLUTTER_TEXTURE_NONE,

                                     &error);

	if (error)
		LOGE("Could not create texure: %s", error->message);
/**/

	actor_data = malloc(sizeof(ActorData));
	actor_data->actor = CLUTTER_ACTOR(desktop);
	actor_data->buffer = xscreendata;
	actor_data->w = Xsize;
	actor_data->h = Ysize;
	actor_data->next = actor_data_root;
	actor_data_root = actor_data;


//	g_signal_connect (desktop, "paint", G_CALLBACK (on_paint), xscreendata);


//  g_signal_connect (desktop, "enter-event", G_CALLBACK (on_enter), NULL);
//  g_signal_connect (desktop, "leave-event", G_CALLBACK (on_leave), NULL);

//  g_signal_connect (desktop, "touch-event", G_CALLBACK (on_touch), NULL);

//  g_signal_connect (desktop, "button-press-event", G_CALLBACK (on_button_press), NULL);
//  g_signal_connect (desktop, "button-release-event", G_CALLBACK (on_button_release), NULL);

  //action = clutter_drag_action_new ();
//  clutter_drag_action_set_drag_threshold (CLUTTER_DRAG_ACTION (action),
//                                          x_drag_threshold,
//                                          y_drag_threshold);
//  clutter_drag_action_set_drag_axis (CLUTTER_DRAG_ACTION (action),
//                                     get_drag_axis (drag_axis));

  //g_signal_connect (desktop, "drag-begin", G_CALLBACK (on_drag_begin), NULL);
  //g_signal_connect (desktop, "drag-end", G_CALLBACK (on_drag_end), NULL);

  //clutter_actor_add_action (desktop, action);

//  clutter_actor_add_effect_with_name (desktop, "disable", clutter_desaturate_effect_new (0.0));
	//clutter_actor_add_effect_with_name (desktop, "curl", 
	//				clutter_page_turn_effect_new (0.0, 45.0, 42.0));



	clutter_actor_add_child (stage, desktop);
	clutter_actor_set_reactive (desktop, TRUE);

	g_signal_connect (desktop_timeline, "new-frame", G_CALLBACK (_update_progress_cb), desktop);

	clutter_timeline_start(desktop_timeline);
#endif

/*
	if(image == NULL){
		image = clutter_image_new();
							
		clutter_actor_set_content_scaling_filters (stage,
									CLUTTER_SCALING_FILTER_TRILINEAR,
									CLUTTER_SCALING_FILTER_LINEAR);
		clutter_actor_set_content_gravity (stage, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);

		clutter_image_set_data (CLUTTER_IMAGE (image),
								xscreendata,
								COGL_PIXEL_FORMAT_RGBA_8888,  // COGL_PIXEL_FORMAT_RGB_888,
								Xsize,
								Ysize,
								Xsize * 4,
								NULL);

		clutter_actor_set_content (stage, image);
		g_object_unref (image);
	}
*/

  if(ioa_startup_log() != 0)
      LOGW("Cannot open /sdcard/Inferno/startup.log");
  amain();


#if 0
  box = mx_box_layout_new_with_orientation (MX_ORIENTATION_VERTICAL);
  mx_box_layout_set_spacing (MX_BOX_LAYOUT (box), 50);

  /* Place it at the center of the stage */

  constraint = clutter_align_constraint_new (stage,
                                             CLUTTER_ALIGN_X_AXIS,
                                             0.5f);
  clutter_actor_add_constraint (box, constraint);

  constraint = clutter_align_constraint_new (stage,
                                             CLUTTER_ALIGN_Y_AXIS,
                                             0.5f);
  clutter_actor_add_constraint (box, constraint);

  clutter_actor_add_child (stage, box);

  square = mx_button_new_with_label ("Fullscreen");
  clutter_actor_add_child (box, square);

  g_signal_connect (square, "clicked",
                    G_CALLBACK (toggle_fullscreen), stage);


  square = mx_button_new_with_label ("Keyboard");
  clutter_actor_add_child (box, square);

  g_signal_connect (square, "clicked",
                    G_CALLBACK (toggle_keyboard), application);

  text = mx_entry_new ();
  mx_entry_set_placeholder (MX_ENTRY (text), "Entry");
  clutter_actor_add_child (box, text);

  mx_focus_manager_push_focus (mx_focus_manager_get_for_stage (CLUTTER_STAGE (stage)),
                               MX_FOCUSABLE (text));
#endif

#if 0
  /* Create a new ClutterTexture */
  hand = clutter_texture_new_from_android_asset (application, "redhand.png");

  vertex.x = clutter_actor_get_width (hand)/2.0;
  vertex.y = clutter_actor_get_height (hand)/2.0;
  vertex.z = 0;

  g_object_set (hand, "rotation-center-x", &vertex, "rotation-center-z",
                &vertex, NULL);
  clutter_actor_set_scale (hand, 2.0, 2.0);
  data->drag_action = clutter_drag_action_new ();
  data->rotate_action = clutter_rotate_action_new ();
  data->zoom_action = clutter_zoom_action_new ();

  clutter_actor_add_action (hand, data->drag_action);
  clutter_actor_add_action (hand, data->rotate_action);
  clutter_actor_add_action (hand, data->zoom_action);

  g_signal_connect (data->rotate_action, "gesture-begin", 
                    G_CALLBACK (_rotate_gesture_begin), data); 
  g_signal_connect (data->rotate_action, "gesture-end", 
                    G_CALLBACK (_rotate_gesture_end), data); 

  clutter_actor_animate (hand, CLUTTER_LINEAR, 4000.0 * 99.0, 
                         "rotation-angle-z", 360.0 * 99.0, 
                         "rotation-angle-x", 180.0 * 99.0, NULL); 
  clutter_actor_add_child (stage, hand);
  clutter_actor_set_reactive (hand, TRUE);
  clutter_actor_set_position (hand,
                              clutter_actor_get_width (stage) / 2 -
                              clutter_actor_get_width (hand),
                              clutter_actor_get_height (stage) / 2 -
                              clutter_actor_get_height (hand));

  /**/
  clone = clutter_clone_new (hand);
  clutter_actor_set_reactive (clone, TRUE);
  clutter_actor_set_position (clone, 0, 0);
  clutter_actor_add_action (clone, clutter_zoom_action_new ());
  clutter_actor_add_child (stage, clone);

  clone = clutter_clone_new (hand);
  clutter_actor_set_reactive (clone, TRUE);
  clutter_actor_set_position (clone,
                              clutter_actor_get_width (stage) - clutter_actor_get_width (clone),
                              0);
  clutter_actor_add_action (clone, clutter_zoom_action_new ());
  clutter_actor_add_child (stage, clone);

  clone = clutter_clone_new (hand);
  clutter_actor_set_reactive (clone, TRUE);
  clutter_actor_set_position (clone,
                              0,
                              clutter_actor_get_height (stage) - clutter_actor_get_height (clone));
  clutter_actor_add_action (clone, clutter_zoom_action_new ());
  clutter_actor_add_child (stage, clone);

  clone = clutter_clone_new (hand);
  clutter_actor_set_reactive (clone, TRUE);
  clutter_actor_set_position (clone,
                              clutter_actor_get_width (stage) - clutter_actor_get_width (clone),
                              clutter_actor_get_height (stage) - clutter_actor_get_height (clone));
  clutter_actor_add_action (clone, clutter_zoom_action_new ());
  clutter_actor_add_child (stage, clone);
#endif

  /**/
  clutter_actor_show (stage);

  return TRUE;
}

#include "inferno_startup.h"


void
clutter_android_main (ClutterAndroidApplication *application)
{
  TestData data;

  ioa_ui_start();

//LOGI("%s: %d", __func__, __LINE__);
  memset (&data, 0, sizeof (TestData));
//LOGI("%s: %d", __func__, __LINE__);
  data.application = application;

  if (application->android_application != NULL && application->android_application->window != NULL){
  	int x, y;
	
	x = ANativeWindow_getWidth (application->android_application->window);
	y = ANativeWindow_getHeight (application->android_application->window);
	
	print("!! Stage_Size: %d x %d", x, y);
  }

  g_signal_connect_after (application, "ready", G_CALLBACK (test_init), &data);
//LOGI("%s: %d", __func__, __LINE__);

  clutter_android_application_run (application);
  ioa_ui_stop();
  ioa_storage_finish();
//LOGI("%s: %d", __func__, __LINE__);
}


/* Traverse the actor list only on its Clutter owner, including full-screen refreshes. */
static void
ioa_render_refresh(void *argument)
{
    ActorData *actor_data;

    (void)argument;
    for(actor_data = actor_data_root; actor_data != NULL; actor_data = actor_data->next){
        if(actor_data->actor != NULL)
            ioa_render_clutter_ext_win_repaint(actor_data->actor);
    }
}


/* Publish a VM refresh through the same owner queue as individual window updates. */
void
int_refresh(int min_x, int min_y, int max_x, int max_y)
{
    (void)min_x; (void)min_y; (void)max_x; (void)max_y;
    ioa_ui_call(ioa_render_refresh, NULL);
}


void ioa_showsplash(){
LOGI("%s: %d", __func__, __LINE__);
}

