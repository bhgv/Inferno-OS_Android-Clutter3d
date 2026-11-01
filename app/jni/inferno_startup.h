#ifndef IOA_INFERNO_STARTUP_H
#define IOA_INFERNO_STARTUP_H // Native Clutter provisioning; included after TestData and stage initialization declarations.
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct IoaStartup IoaStartup;

struct IoaStartup {
	pthread_mutex_t lock; // Protect worker status and cancellation; never held across UI or file IO.
	pthread_t worker; // One native extraction worker, joined before the activity's asset manager can retire.
	AAssetManager *manager; // Activity-owned asset manager retained by the enclosing native application loop.
	ClutterAndroidApplication *application; // UI-thread-only continuation context.
	TestData *data; // Enclosing clutter_android_main storage, live until worker join.
	ClutterActor *label, *bar, *panel; // UI-thread-owned progress actors; worker never touches Clutter.
	unsigned int done, total; // Completed manifest file entries and complete known file count.
	int booted, started, finished, cancelled, result, error; // Serialized worker lifecycle and final errno.
	guint timer; // UI progress polling source; removed before teardown.
	char path[256]; // Bounded current asset display name.
};

static IoaStartup storage_startup = { .lock = PTHREAD_MUTEX_INITIALIZER }; // Single native activity provisioning session.


/* Publish progress without touching the GLES context or the Android event loop from the worker. */
static int
ioa_storage_progress(unsigned int done, unsigned int total, const char *path, void *argument)
{
	IoaStartup *state = argument;
	int keep;

	pthread_mutex_lock(&state->lock);
	state->done = done; state->total = total;
	snprintf(state->path, sizeof(state->path), "%s", path);
	keep = !state->cancelled;
	pthread_mutex_unlock(&state->lock);
	return keep;
}


/* Keep all APK traversal and shared-storage writes off the Clutter UI thread. */
static void*
ioa_storage_worker(void *argument)
{
	IoaStartup *state = argument;
	int result, saved;

	result = ioa_prepare_storage_progress(state->manager, ioa_storage_progress, state);
	saved = errno;
	pthread_mutex_lock(&state->lock);
	state->result = result; state->error = saved; state->finished = 1;
	pthread_mutex_unlock(&state->lock);
	return NULL;
}


/* Cancel pending extraction and stop polling before any progress actor is destroyed. */
static void
ioa_storage_destroyed(ClutterActor *actor, gpointer argument)
{
	IoaStartup *state = argument;
	(void)actor;

	pthread_mutex_lock(&state->lock);
	state->cancelled = 1;
	pthread_mutex_unlock(&state->lock);
	if(state->timer != 0){
		g_source_remove(state->timer);
		state->timer = 0;
	}
	state->panel = state->label = state->bar = NULL;
}


/* Draw counts on the UI thread, then start Inferno only after the complete tree passed validation. */
static gboolean
ioa_storage_tick(gpointer argument)
{
	IoaStartup *state = argument;
	char message[1024], path[256];
	unsigned int done, total, percent;
	int finished, result, saved, cancelled;
	float width, top;

	pthread_mutex_lock(&state->lock);
	done = state->done; total = state->total; finished = state->finished;
	result = state->result; saved = state->error; cancelled = state->cancelled;
	memcpy(path, state->path, sizeof(path));
	pthread_mutex_unlock(&state->lock);
	if(cancelled){ state->timer = 0; return FALSE; }
	percent = total == 0 ? 0 : (unsigned int)((100ULL*done)/total);
	width = clutter_actor_get_width(stage);
    top = state->application == NULL ? 112 : state->application->android_application->contentRect.top + 24;
    if(top < 64) top = 64;
    clutter_actor_set_position(state->bar, 16, top);
    clutter_actor_set_position(state->label, 16, top+24);
	clutter_actor_set_size(state->panel, width, clutter_actor_get_height(stage));
	clutter_actor_set_size(state->bar, (width > 32 ? width-32 : 1)*percent/100, 16);
	if(finished && result != 0)
		snprintf(message, sizeof(message),
            saved == EACCES || saved == EPERM ?
            "Storage access denied (%d).\nCannot create/read /sdcard/Inferno.\nAndroid Settings > Apps > Inferno GLES > Permissions: allow Files / Storage.\nOn newer Android also check Special app access > All files access.\nThen close and reopen Inferno.\nNo VM has been started.\n%s" :
            "Inferno startup failed (%d).\nCheck /sdcard/Inferno structure, free space and bundled runtime.\nClose and reopen after correcting the problem.\n%s", saved, path);
	else
		snprintf(message, sizeof(message), "Preparing /sdcard/Inferno\n%u%%  (%u / %u files)\n%s", percent, done, total, path);
	clutter_actor_set_width(state->label, width > 32 ? width-32 : 1);
	clutter_text_set_text(CLUTTER_TEXT(state->label), message);
	if(!finished)
		return TRUE;
	state->timer = 0;
	if(result == 0){
		clutter_actor_destroy(state->panel);
		state->panel = state->label = state->bar = NULL;
		state->booted = start_inferno_stage(state->application, state->data);
		if(!state->booted){
			ClutterActor *failure = clutter_text_new_with_text("Sans 16", "Inferno could not start. Check runtime files and native logs.");
			clutter_actor_add_child(stage, failure);
			clutter_actor_show(stage);
		}
	}
	return FALSE;
}


/* Reuse an existing complete tree immediately; otherwise show native progress and launch one worker. */
static gboolean
test_init(ClutterAndroidApplication *application, TestData *data)
{
	IoaStartup *state = &storage_startup;
	ClutterColor background = {32, 32, 32, 255}, fill = {80, 180, 240, 255}, white = {255, 255, 255, 255};
	int status;

	if(clutter_init(NULL, NULL) <= 0)
		return FALSE;
	stage = clutter_stage_get_default();
	if(state->booted || state->started || state->panel != NULL)
		return TRUE;
	if(ioa_runtime_ready()){
		if(ioa_prepare_storage(NULL) != 0)
			return FALSE;
		state->booted = start_inferno_stage(application, data);
		return state->booted;
	}
	if(application->android_application == NULL || application->android_application->activity == NULL ||
	   application->android_application->window == NULL)
		return FALSE;
	state->application = application; state->data = data;
	state->manager = application->android_application->activity->assetManager;
	state->panel = clutter_actor_new();
	clutter_actor_set_background_color(state->panel, &background);
	clutter_actor_add_child(stage, state->panel);
	state->label = clutter_text_new_with_text("Sans 16", "Checking bundled Inferno files...");
	clutter_text_set_color(CLUTTER_TEXT(state->label), &white);
	clutter_text_set_line_wrap(CLUTTER_TEXT(state->label), TRUE);
	clutter_actor_set_position(state->label, 16, 136);
	clutter_actor_add_child(state->panel, state->label);
	state->bar = clutter_actor_new();
	clutter_actor_set_background_color(state->bar, &fill);
	clutter_actor_set_position(state->bar, 16, 112);
	clutter_actor_add_child(state->panel, state->bar);
	clutter_actor_set_size(stage, ANativeWindow_getWidth(application->android_application->window),
	                               ANativeWindow_getHeight(application->android_application->window));
	clutter_actor_show(stage);
	ioa_storage_tick(state);
	g_signal_connect(stage, "destroy", G_CALLBACK(ioa_storage_destroyed), state);
	status = pthread_create(&state->worker, NULL, ioa_storage_worker, state);
	if(status != 0){
		clutter_text_set_text(CLUTTER_TEXT(state->label), "Cannot start native storage worker");
		return TRUE;
	}
	state->started = 1;
	state->timer = g_timeout_add(50, ioa_storage_tick, state);
	return TRUE;
}


/* Join before returning from the native activity loop so the asset-manager borrow remains valid. */
static void
ioa_storage_finish(void)
{
	IoaStartup *state = &storage_startup;

	if(!state->started)
		return;
	pthread_mutex_lock(&state->lock);
	state->cancelled = 1;
	pthread_mutex_unlock(&state->lock);
	if(state->timer != 0){ g_source_remove(state->timer); state->timer = 0; }
	pthread_join(state->worker, NULL);
	state->started = 0;
}
#endif
