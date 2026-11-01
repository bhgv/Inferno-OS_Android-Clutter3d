#include "inferno_viewport.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1 // Match the boolean interface used by extracted Clutter callbacks.
#define FALSE 0 // No event or unavailable geometry.
#define CLUTTER_EVENT_STOP 1 // One stage capture handles each touch exactly once.
#define CLUTTER_EVENT_PROPAGATE 0 // Unrelated events continue through Clutter.
#define CLUTTER_POINTER_DEVICE 0 // One bounded host pointer-device service.
#define CLUTTER_STAGE_STATE_ACTIVATED 1 // Active stage bit tested with the actual unset/set contract.
#define AMOTION_EVENT_ACTION_MASK 255 // Android low-byte action tag.
#define AMOTION_EVENT_ACTION_POINTER_INDEX_MASK 0xff00 // Index is separate from stable pointer identity.
#define AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT 8 // Android action index shift.
#define AMOTION_EVENT_ACTION_DOWN 0 // First touch begins.
#define AMOTION_EVENT_ACTION_UP 1 // Last touch ends.
#define AMOTION_EVENT_ACTION_MOVE 2 // Existing contacts move.
#define AMOTION_EVENT_ACTION_CANCEL 3 // All contacts cancel.
#define AMOTION_EVENT_ACTION_OUTSIDE 4 // Outside delivery cancels contacts.
#define AMOTION_EVENT_ACTION_POINTER_DOWN 5 // Another contact begins.
#define AMOTION_EVENT_ACTION_POINTER_UP 6 // One nonfinal contact ends.
#define CLUTTER_ACTOR(actor) (actor) // Host stage and actor share their bounded representation.
#define CLUTTER_STAGE_COGL(window) (window) // Bounded onscreen owner, not an EGL implementation.
#define LOGI(...) ((void)0) // Parent callback diagnostics do not implement input behavior.

typedef int gboolean;
typedef unsigned int guint, guint32;
typedef unsigned long long guint64;
typedef uintptr_t guintptr;
typedef void *gpointer;
typedef float gfloat;

enum {
	CLUTTER_TOUCH_BEGIN, CLUTTER_TOUCH_UPDATE, CLUTTER_TOUCH_END, CLUTTER_TOUCH_CANCEL,
	CLUTTER_OTHER_EVENT
};
typedef int ClutterEventType;

typedef struct ClutterStageCogl {
	void *onscreen; // Non-null fake surface handle passed to the size observer.
} ClutterStageCogl;
typedef ClutterStageCogl ClutterStageWindow;

typedef struct ClutterActor {
	double x, y, sx, sy; // Affine canvas placement used by the host inverse-transform adapter.
	int width, height, visible; // Observed actor allocation and visibility.
	ClutterStageWindow *window; // Borrowed stage window for backend synchronization.
} ClutterActor;
typedef ClutterActor ClutterStage;

typedef struct ClutterEvent {
	int type; // Touch type dispatched by the actual backend translation body.
	struct {
		guint32 time; // Clutter milliseconds, including its natural uint32 wrap.
		float x, y; // Raw stage coordinates before the captured-event callback.
		void *device, *sequence; // Borrowed pointer service and stable contact identity.
		int modifier_state; // Modifier state forwarded by the backend.
	} touch;
	struct { ClutterStage *stage; } any; // Stage receiving this queued event.
} ClutterEvent;
typedef ClutterEvent ClutterTouchEvent;

typedef struct ARect {
	int left, top, right, bottom; // Android content area in native-window coordinates.
} ARect;
typedef struct NativeWindow {
	int width, height; // Actual surface dimensions returned by native window accessors.
} NativeWindow;
struct android_app {
	NativeWindow *window; // Borrowed window; null simulates window loss.
	ARect contentRect; // Current Android app content rectangle.
};
typedef struct AInputEvent {
	int action, count; // Encoded native action and number of active pointer records.
	int ids[16]; // Stable IDs, deliberately unrelated to current record order.
	float x[16], y[16]; // Native window coordinates.
	int64_t time; // Android monotonic nanoseconds.
} AInputEvent;
typedef struct ClutterAndroidApplication {
	int modifier_state; // Modifier value passed to every translated touch.
} ClutterAndroidApplication;
typedef int ClutterDeviceManager;
typedef int ClutterInputDevice;
typedef struct ClutterBackendAndroid {
	void *android_source; // Queue observer context.
} ClutterBackendAndroid;

static ClutterActor stage_actor, canvas_actor; // Bounded display services used by actual application callbacks.
static ClutterActor *stage = &stage_actor, *inferno_canvas = &canvas_actor;
static struct android_app app; // One native app owner for resize/content/window scenarios.
static struct android_app *aapp = &app;
static IoaViewport inferno_viewport;
static IoaPointer inferno_pointer;
static int Xsize = 540, Ysize = 960; // Immutable logical desktop for the integration oracle.
static int checks, deliveries; // Active predicates and published mouse-event observations.
static int surface_width, surface_height, relayouts, redraws, activated; // Backend geometry/focus observations.
static IoaPointerEvent last; // Actual event emitted to Inferno's mouse boundary.
static ClutterEvent queue[64]; // Detached events produced by the actual translation function.
static int queued;
static ClutterDeviceManager manager;
static ClutterInputDevice device;
static ClutterBackendAndroid backend;


/* Preserve all behavioral predicates under NDEBUG and locate an independent failure. */
static void
check(int condition)
{
	if(!condition){
		fprintf(stderr, "GLES input predicate failed after %d checks\n", checks);
		abort();
	}
	checks++;
}


/* Record the complete Inferno event rather than reimplementing its dispatch decisions. */
static void
xmouse_btn(int x, int y, int buttons)
{
	last.x = x;
	last.y = y;
	last.buttons = buttons;
	deliveries++;
}


/* Expose bounded native geometry without pretending to create an Android window. */
static int ANativeWindow_getWidth(NativeWindow *window) { return window->width; }


/* Match the second independent dimension read by both production geometry paths. */
static int ANativeWindow_getHeight(NativeWindow *window) { return window->height; }


/* Observe actual actor placement calls used to display the shared canvas. */
static void clutter_actor_set_position(ClutterActor *actor, double x, double y) { actor->x=x; actor->y=y; }


/* Observe the same uniform scale whose inverse must be used for picking. */
static void clutter_actor_set_scale(ClutterActor *actor, double x, double y) { actor->sx=x; actor->sy=y; }


/* Observe backend allocation publication without performing any GPU operation. */
static void clutter_actor_set_size(ClutterActor *actor, int w, int h) { actor->width=w; actor->height=h; }


/* Observe suppression of input when the content area is unusable. */
static void clutter_actor_hide(ClutterActor *actor) { actor->visible=0; }


/* Observe restoration after a valid surface returns. */
static void clutter_actor_show(ClutterActor *actor) { actor->visible=1; }


/* Retain raw Clutter event coordinates until the actual callback requests transformation. */
static void clutter_event_get_coords(ClutterEvent *event, float *x, float *y) { *x=event->touch.x; *y=event->touch.y; }


/* Preserve the stable sequence accessor independently of pointer-array indices. */
static void *clutter_event_get_event_sequence(ClutterEvent *event) { return event->touch.sequence; }


/* Preserve event type filtering from the real captured-event callback. */
static int clutter_event_type(ClutterEvent *event) { return event->type; }


/* Supply the one stage used by the actual backend's public stage lookup. */
static ClutterStage *clutter_stage_manager_get_default_stage(void *unused) { (void)unused; return stage; }


/* Supply an opaque manager identity without reproducing any backend decisions. */
static void *clutter_stage_manager_get_default(void) { return &manager; }


/* Provide the fake surface owner attached to the bounded host stage. */
static ClutterStageWindow *_clutter_stage_get_window(ClutterStage *actor) { return actor->window; }


/* Observe both dimensions given to Cogl's surface-resize boundary. */
static void cogl_android_onscreen_update_size(void *screen, int w, int h) { check(screen!=NULL); surface_width=w; surface_height=h; }


/* Count relayout requests separately from render-surface size publication. */
static void clutter_actor_queue_relayout(ClutterActor *actor) { check(actor==stage); relayouts++; }


/* Count redraw requests after consistent backend geometry publication. */
static void clutter_actor_queue_redraw(ClutterActor *actor) { check(actor==stage); redraws++; }


/* Keep the exact set-then-unset contract of _clutter_stage_update_state. */
static void _clutter_stage_update_state(ClutterStage *actor, int unset, int set) { check(actor==stage); activated=(activated|set)&~unset; }


/* Supply a pointer device to the complete actual native translator. */
static ClutterDeviceManager *clutter_device_manager_get_default(void) { return &manager; }


/* Keep the requested device kind checked at its lookup boundary. */
static ClutterInputDevice *clutter_device_manager_get_core_device(ClutterDeviceManager *m,int kind) { check(m==&manager && kind==CLUTTER_POINTER_DEVICE); return &device; }


/* Supply the native source queue owner used by the translator. */
static void *clutter_get_default_backend(void) { return &backend; }


/* Observe device-to-stage binding before native touch events are queued. */
static void _clutter_input_device_set_stage(ClutterInputDevice *d,ClutterStage *s) { check(d==&device && s==stage); }


/* Allocate a real detached event so the observer never aliases the next translation. */
static ClutterEvent *clutter_event_new(int type) { ClutterEvent *e=calloc(1,sizeof(*e)); check(e!=NULL); e->type=type; return e; }


/* Copy events at the backend boundary, leaving actual type/ID/time decisions untouched. */
static void _clutter_event_source_android_push_event(void *source,ClutterEvent *event) { (void)source; check(queued<64); queue[queued++]=*event; free(event); }


/* Read the complete encoded Android action without decoding it in the adapter. */
static int AMotionEvent_getAction(AInputEvent *event) { return event->action; }


/* Provide the current array count independently of persistent pointer IDs. */
static int AMotionEvent_getPointerCount(AInputEvent *event) { return event->count; }


/* Preserve the native nanosecond clock for the production unit conversion. */
static int64_t AMotionEvent_getEventTime(AInputEvent *event) { return event->time; }


/* Supply IDs deliberately chosen to expose index/identity confusion. */
static int AMotionEvent_getPointerId(AInputEvent *event,int index) { return event->ids[index]; }


/* Expose window-space X from the current pointer record. */
static float AMotionEvent_getX(AInputEvent *event,int index) { return event->x[index]; }


/* Expose window-space Y from the current pointer record. */
static float AMotionEvent_getY(AInputEvent *event,int index) { return event->y[index]; }

/* The sandbox hides /proc; disable leak scanning only, retaining ASan memory checks. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

IMPLEMENTATION


/* Deliver one staged event through the complete actual application callback. */
static void
touch(int type, uintptr_t sequence, double x, double y)
{
	ClutterEvent event;

	memset(&event,0,sizeof(event));
	event.type=type;
	event.touch.sequence=(void*)sequence;
	event.touch.x=x;
	event.touch.y=y;
	check(on_touch(stage,&event,NULL)==CLUTTER_EVENT_STOP);
}


/* Compare coordinate and mask outcomes against independent physical/logical examples. */
int
main(void)
{
	NativeWindow window={1080,2000};
	ClutterStageCogl stage_window={(void*)1};
	AInputEvent input;
	IoaViewport view, before;
	size_t bytes;
	int old, x, y, width, height, ix, iy;

	app.window=&window;
	app.contentRect=(ARect){0,40,1080,1960};
	stage->window=&stage_window;
	sync_surface_geometry();
	check(canvas_actor.sx==2 && canvas_actor.sy==2 && canvas_actor.x==0 && canvas_actor.y==40);
	touch(CLUTTER_TOUCH_BEGIN,9,200,440);
	check(last.x==100 && last.y==200 && last.buttons==1);
	touch(CLUTTER_TOUCH_BEGIN,77,1000,1800);
	check(last.x==100 && last.y==200 && last.buttons==2);
	touch(CLUTTER_TOUCH_BEGIN,123,800,1200);
	check(last.buttons==4);
	old=deliveries;
	touch(CLUTTER_TOUCH_BEGIN,9,300,400);
	check(deliveries==old);
	touch(CLUTTER_TOUCH_UPDATE,77,700,1000);
	check(last.x==100 && last.y==200 && last.buttons==4);
	touch(CLUTTER_TOUCH_END,123,800,1200);
	check(last.buttons==2);
	touch(CLUTTER_TOUCH_END,77,700,1000);
	check(last.buttons==1);
	touch(CLUTTER_TOUCH_END,9,-100,9999);
	check(last.x==0 && last.y==959 && last.buttons==0 && inferno_pointer.count==0);
	old=deliveries;
	touch(CLUTTER_TOUCH_END,9,200,440);
	check(deliveries==old);
	touch(CLUTTER_TOUCH_BEGIN,5,200,440);
	touch(CLUTTER_TOUCH_BEGIN,6,300,440);
	touch(CLUTTER_TOUCH_CANCEL,5,200,440);
	check(last.buttons==0 && inferno_pointer.count==0);
	old=deliveries;
	touch(CLUTTER_TOUCH_CANCEL,6,200,440);
	check(deliveries==old);
	touch(CLUTTER_TOUCH_BEGIN,11,200,440);
	window.width=2000; window.height=1080;
	app.contentRect=(ARect){20,20,1980,1060};
	sync_surface_geometry();
	check(last.buttons==0 && Xsize==540 && Ysize==960 && canvas_actor.sx==canvas_actor.sy);
	touch(CLUTTER_TOUCH_BEGIN,12,canvas_actor.x+122.5*canvas_actor.sx,canvas_actor.y+312.5*canvas_actor.sy);
	check(last.x==122 && last.y==312 && last.buttons==1);
	on_stage_deactivate(stage,NULL);
	check(last.buttons==0);
	old=deliveries;
	touch(CLUTTER_TOUCH_BEGIN,13,0,0);
	check(deliveries==old && inferno_pointer.count==0);
	touch(CLUTTER_TOUCH_BEGIN,13,1000,500);
	touch(CLUTTER_TOUCH_END,13,1000,500);
	check(last.buttons==0 && inferno_pointer.count==0);
	touch(CLUTTER_TOUCH_BEGIN,14,1000,500);
	app.window=NULL;
	sync_surface_geometry();
	check(last.buttons==0 && inferno_viewport.scale==0);
	app.window=&window;
	sync_surface_geometry();
	app.contentRect=(ARect){30,30,30,70};
	sync_surface_geometry();
	check(!canvas_actor.visible && inferno_viewport.scale==0);
	old=deliveries;
	touch(CLUTTER_TOUCH_BEGIN,15,1000,500);
	check(deliveries==old);
	app.contentRect=(ARect){0,0,0,0};
	sync_surface_geometry();
	check(canvas_actor.visible);

	update_android_stage_geometry(&app);
	check(stage->width==2000 && stage->height==1080 && surface_width==2000 && surface_height==1080);
	check(relayouts==1 && redraws==1);
	update_android_stage_focus(TRUE);
	check(activated==CLUTTER_STAGE_STATE_ACTIVATED);
	update_android_stage_focus(FALSE);
	check(activated==0);

	memset(&input,0,sizeof(input));
	input.count=3; input.ids[0]=27; input.ids[1]=4; input.ids[2]=19;
	input.x[0]=1000; input.x[1]=1010; input.x[2]=1020;
	input.y[0]=500; input.y[1]=510; input.y[2]=520;
	input.time=123456789000LL;
	input.action=AMOTION_EVENT_ACTION_POINTER_DOWN|(2<<8);
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	check(queued==3 && queue[0].type==CLUTTER_TOUCH_UPDATE && queue[2].type==CLUTTER_TOUCH_BEGIN);
	check(queue[0].touch.sequence==(void*)28 && queue[1].touch.sequence==(void*)5 && queue[2].touch.sequence==(void*)20);
	check(queue[0].touch.time==123456 && queue[2].touch.x==1020);
	queued=0; input.action=AMOTION_EVENT_ACTION_CANCEL;
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	check(queued==3);
	for(ix=0;ix<queued;ix++)check(queue[ix].type==CLUTTER_TOUCH_CANCEL);
	queued=0; input.action=AMOTION_EVENT_ACTION_POINTER_UP|(8<<8);
	check(!translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input) && queued==0);

	/* Replay actual native translation into the actual captured-event callback. */
	queued=0; input.count=1; input.ids[0]=27;
	input.x[0]=1000; input.y[0]=500;
	input.action=AMOTION_EVENT_ACTION_DOWN;
	input.time=(4294967296LL+17)*1000000;
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	check(queued==1 && queue[0].touch.time==17);
	check(on_touch(stage,&queue[0],NULL)==CLUTTER_EVENT_STOP);
	check(last.x==270 && last.y==444 && last.buttons==1);
	queued=0; input.count=2; input.ids[1]=4; input.x[1]=1100; input.y[1]=600;
	input.action=AMOTION_EVENT_ACTION_POINTER_DOWN|(1<<8);
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	for(ix=0;ix<queued;ix++)on_touch(stage,&queue[ix],NULL);
	check(last.x==270 && last.y==444 && last.buttons==2);
	queued=0; input.ids[0]=4; input.ids[1]=27;
	input.x[0]=1100; input.y[0]=600; input.x[1]=1000; input.y[1]=500;
	input.action=AMOTION_EVENT_ACTION_MOVE;
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	for(ix=0;ix<queued;ix++)on_touch(stage,&queue[ix],NULL);
	check(last.x==270 && last.y==444 && last.buttons==2);
	queued=0; input.action=AMOTION_EVENT_ACTION_POINTER_UP|(1<<8);
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	for(ix=0;ix<queued;ix++)on_touch(stage,&queue[ix],NULL);
	check(last.x==358 && last.y==533 && last.buttons==1);
	queued=0; input.action=AMOTION_EVENT_ACTION_CANCEL;
	check(translate_motion_event_to_touch_event(&(ClutterAndroidApplication){0},&input));
	old=deliveries;
	for(ix=0;ix<queued;ix++)on_touch(stage,&queue[ix],NULL);
	check(deliveries==old+1 && last.buttons==0);
	for(ix=0;ix<IOA_MAX_TOUCHES;ix++)touch(CLUTTER_TOUCH_BEGIN,ix+1,1000,500);
	check(inferno_pointer.count==IOA_MAX_TOUCHES && last.buttons==4);
	old=deliveries;
	touch(CLUTTER_TOUCH_BEGIN,999,1000,500);
	check(deliveries==old && inferno_pointer.count==IOA_MAX_TOUCHES);
	touch(CLUTTER_TOUCH_CANCEL,999,1000,500);
	check(last.buttons==0 && inferno_pointer.count==0);

	for(width=1;width<=2048;width*=2){
		for(height=1;height<=2048;height*=2){
			check(ioa_viewport_fit(&view,width,height,1920,1080,10,30,1910,1050));
			check(view.available_width==1900 && view.available_height==1020);
			for(ix=0;ix<3;ix++)for(iy=0;iy<3;iy++){
				double lx=(width-1)*ix/2.0+0.25, ly=(height-1)*iy/2.0+0.25;
				check(ioa_viewport_point(&view,view.x+lx*view.scale,view.y+ly*view.scale,0,&x,&y));
				check(x==(int)lx && y==(int)ly);
			}
			check(!ioa_viewport_point(&view,view.x-1,view.y,0,&x,&y));
			check(ioa_viewport_point(&view,-9999,99999,1,&x,&y) && x==0 && y==height-1);
		}
	}
	before=view;
	check(!ioa_viewport_fit(&view,540,960,0,0,0,0,0,0) && memcmp(&view,&before,sizeof(view))==0);
	check(!ioa_viewport_point(&view,NAN,0,0,&x,&y));
	check(!ioa_viewport_point(&view,INFINITY,0,1,&x,&y));
	check(ioa_surface_bytes(1080,1920,&bytes) && bytes==1080u*1920*4);
	check(!ioa_surface_bytes(0,1920,&bytes) && !ioa_surface_bytes(2147483647,2147483647,&bytes));
	printf("PASS GLES input/geometry: %d active predicates; actual callbacks/backend translator, bounded Clutter/Android interfaces\n",checks);
	return 0;
}
