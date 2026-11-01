#!/usr/bin/env python3
"""Run the owning Clutter startup worker with real pthreads and thread-checking bounded UI actors."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "inferno_storage.h"
typedef int gboolean;
typedef unsigned int guint;
typedef void *gpointer;
typedef struct { unsigned char r,g,b,a; } ClutterColor;
typedef struct { float width,height; char text[1024]; } ClutterActor;
typedef struct { AAssetManager *assetManager; } Activity;
typedef struct { int left,top,right,bottom; } Rect;
typedef struct { Activity *activity; void *window; Rect contentRect; } AndroidApp;
typedef struct { AndroidApp *android_application; } ClutterAndroidApplication;
typedef struct { int unused; } TestData;
#define TRUE 1 // GLib boolean boundary.
#define FALSE 0 // GLib boolean boundary.
#define CLUTTER_TEXT(p) (p) // Stub actor type conversion.
#define G_CALLBACK(p) ((void*)(p)) // Signal callback retained only by the production UI loop.
static ClutterActor root = {320,640,{0}}, *stage;
static pthread_t owner;
static int ready, fail, boots, assetcalls, uicalls;


/* Keep every actor operation on the simulated Clutter UI thread. */
static void ui(void) { if(!pthread_equal(owner,pthread_self())) abort(); uicalls++; }


/* Count the existing ready-tree path without touching packaged assets. */
int ioa_runtime_ready(void) { return ready; }


/* The ready-tree continuation receives no APK manager. */
int ioa_prepare_storage(AAssetManager *manager) { if(manager) abort(); return 0; }


/* A real worker delivers monotonic progress, delay, failure and cancellation. */
int ioa_prepare_storage_progress(AAssetManager *m,IoaStorageProgress cb,void *arg)
{
    unsigned int i;
    (void)m;
    if(pthread_equal(owner,pthread_self())) abort();
    assetcalls++;
    for(i=0;i<=3;i++){
        if(!cb(i,3,"dis/wm/awm.dis",arg)) return -1;
        usleep(20000);
    }
    if(fail){ errno=fail; return -1; }
    return 0;
}


/* Resume actual stage creation only from the UI thread after preparation. */
static int start_inferno_stage(ClutterAndroidApplication *a,TestData *d) { (void)a;(void)d;ui();boots++;return 1; }


/* Bound toolkit operations while checking all their thread ownership. */
static int clutter_init(void *a,void *b){(void)a;(void)b;ui();return 1;}
static ClutterActor *clutter_stage_get_default(void){ui();return &root;}
static ClutterActor *clutter_actor_new(void){ui();return calloc(1,sizeof(ClutterActor));}
static ClutterActor *clutter_text_new_with_text(const char *f,const char *t){ClutterActor *a; (void)f;ui();a=clutter_actor_new();snprintf(a->text,sizeof(a->text),"%s",t);return a;}
static float clutter_actor_get_width(ClutterActor *a){ui();return a->width;}
static float clutter_actor_get_height(ClutterActor *a){ui();return a->height;}
static void clutter_actor_set_size(ClutterActor *a,float w,float h){ui();a->width=w;a->height=h;}
static void clutter_actor_set_width(ClutterActor *a,float w){ui();a->width=w;}
static void clutter_text_set_text(ClutterActor *a,const char *t){ui();snprintf(a->text,sizeof(a->text),"%s",t);}
static void clutter_actor_destroy(ClutterActor *a){(void)a;ui();}
static void clutter_actor_set_background_color(ClutterActor *a,const ClutterColor *c){(void)a;(void)c;ui();}
static void clutter_actor_add_child(ClutterActor *a,ClutterActor *c){(void)a;(void)c;ui();}
static void clutter_actor_show(ClutterActor *a){(void)a;ui();}
static void clutter_text_set_color(ClutterActor *a,const ClutterColor *c){(void)a;(void)c;ui();}
static void clutter_text_set_line_wrap(ClutterActor *a,int w){(void)a;(void)w;ui();}
static void clutter_actor_set_position(ClutterActor *a,float x,float y){(void)a;(void)x;(void)y;ui();}
static int ANativeWindow_getWidth(void *w){(void)w;return 320;}
static int ANativeWindow_getHeight(void *w){(void)w;return 640;}
static void g_source_remove(guint id){(void)id;ui();}
static guint g_timeout_add(int ms,gboolean (*cb)(gpointer),void *a){(void)ms;(void)cb;(void)a;ui();return 1;}
static void g_signal_connect(ClutterActor *a,const char *n,void *cb,void *p){(void)a;(void)n;(void)cb;(void)p;ui();}
#include "inferno_startup.h"


/* Reset only after the real worker was joined, simulating another native activity session. */
static void reset(void)
{
    pthread_mutex_destroy(&storage_startup.lock);
    memset(&storage_startup,0,sizeof(storage_startup));
    pthread_mutex_init(&storage_startup.lock,NULL);
    boots=assetcalls=0;
}


/* Pump the UI while the worker runs: no nested main-context dispatch or UI calls from pthreads. */
static void pump(void)
{
    int i, running=1;
    for(i=0;i<5000;i++){
        running=ioa_storage_tick(&storage_startup);
        if(!running)break;
        usleep(1000);
    }
    if(running || i<2)abort();
}


/* Exercise immediate reuse, successful asynchronous extraction, visible failure and destroy cancellation. */
int main(void)
{
    Activity activity={NULL}; AndroidApp app={&activity,(void*)1,{0,80,320,640}};
    ClutterAndroidApplication application={&app}; TestData data={0};
    owner=pthread_self();ready=1;
    if(!test_init(&application,&data)||boots!=1||assetcalls||storage_startup.started)abort();
    if(!test_init(&application,&data)||boots!=1)abort();
    reset();ready=0;
    if(!test_init(&application,&data))abort();
    pump();ioa_storage_finish();
    if(boots!=1||assetcalls!=1||storage_startup.done!=3||storage_startup.total!=3)abort();
    reset();fail=EIO;
    if(!test_init(&application,&data))abort();
    pump();
    if(boots||!strstr(storage_startup.label->text,"failed"))abort();
    ioa_storage_finish();reset();fail=EACCES;
    if(!test_init(&application,&data))abort();
    pump();
    if(boots || !strstr(storage_startup.label->text,"Storage access denied") ||
       !strstr(storage_startup.label->text,"Settings") || !strstr(storage_startup.label->text,"reopen"))abort();
    ioa_storage_finish();reset();fail=0;
    if(!test_init(&application,&data))abort();
    ioa_storage_destroyed(stage,&storage_startup);ioa_storage_finish();
    if(boots||storage_startup.started||!storage_startup.cancelled)abort();
    puts("PASS native startup: ready-tree bypass, responsive progress, UI-thread-only actors, error and cancellation/join");
    return 0;
}
'''


# Compile the actual production worker/controller header; rendering itself remains a bounded toolkit boundary.
def main():
    with tempfile.TemporaryDirectory(prefix='startup-progress-', dir=ROOT) as directory:
        work = Path(directory); source = work/'test.c'; source.write_text('#include <errno.h>\n'+SOURCE)
        binary = work/'test'
        subprocess.run(['cc','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread','-I'+str(ROOT/'app/jni'),str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=10)


if __name__ == '__main__':
    main()
