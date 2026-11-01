#!/usr/bin/env python3
"""Run actual native UI job lifetime code with real worker threads and a bounded idle-source queue."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
typedef unsigned int guint;
typedef int gboolean;
typedef void *gpointer;
#define FALSE 0 // GLib source removal result.
#define G_PRIORITY_DEFAULT_IDLE 200 // Let input and paint run before worker jobs.
struct Source {guint id; gboolean (*fn)(gpointer);void *arg;};
static struct Source queue[32];
static pthread_mutex_t queue_lock=PTHREAD_MUTEX_INITIALIZER;
static guint serial;
static int calls,done,result,refreshes;
static pthread_t owner;


/* Model queued idles, never an immediate callback on the submitting worker. */
static guint g_idle_add_full(int priority,gboolean (*fn)(gpointer),void *arg,void *destroy)
{
    unsigned int i;
    (void)destroy;if(priority!=200)abort();
    pthread_mutex_lock(&queue_lock);
    for(i=0;i<32&&queue[i].id;i++);
    if(i==32)abort();
    queue[i]=(struct Source){++serial,fn,arg};
    pthread_mutex_unlock(&queue_lock);
    return serial;
}


/* Remove a still-pending source before its worker stack is released. */
static int g_source_remove(guint id)
{
    unsigned int i;int found=0;
    pthread_mutex_lock(&queue_lock);
    for(i=0;i<32;i++)if(queue[i].id==id){queue[i].id=0;found=1;}
    pthread_mutex_unlock(&queue_lock);return found;
}
#include "inferno_ui_queue.h"
/* ACTUAL_REFRESH */


/* Observe the actual callback thread and the worker's still-live argument storage. */
static void operation(void *arg)
{
    if(!pthread_equal(pthread_self(),owner))abort();
    (*(int*)arg)++;calls++;
}


/* Submit from a real VM-like thread; no renderer operation may run here. */
static void *worker(void *unused)
{
    int value=17;(void)unused;
    result=ioa_ui_call(operation,&value);
    if(result&&value!=18)abort();
    if(result)int_refresh(0,0,10,10);
    if(!result&&value!=17)abort();
    __atomic_store_n(&done,1,__ATOMIC_RELEASE);return NULL;
}


/* Take one queued source on the owning UI thread. */
static int pump(void)
{
    unsigned int i;struct Source source={0};
    pthread_mutex_lock(&queue_lock);
    for(i=0;i<32;i++)if(queue[i].id){source=queue[i];queue[i].id=0;break;}
    pthread_mutex_unlock(&queue_lock);
    if(source.id)source.fn(source.arg);
    return source.id!=0;
}


/* Execute, cancel, reject after shutdown and reopen without any orphaned idle source. */
int main(void)
{
    pthread_t thread;int i,local=0;
    owner=pthread_self();ioa_ui_start();
    if(!ioa_ui_call(operation,&local)||local!=1)abort();
    for(i=0;i<100;i++){
        done=0;if(pthread_create(&thread,NULL,worker,NULL))abort();
        while(!__atomic_load_n(&done,__ATOMIC_ACQUIRE)){pump();usleep(100);}
        pthread_join(thread,NULL);if(result!=1)abort();
    }
    done=0;pthread_create(&thread,NULL,worker,NULL);
    for(i=0;i<10000;i++){
        int pending;
        pthread_mutex_lock(&ioa_ui_mutex);pending=ioa_ui_pending!=NULL;pthread_mutex_unlock(&ioa_ui_mutex);
        if(pending)break;
        usleep(100);
    }
    if(i==10000)abort();
    ioa_ui_stop();pthread_join(thread,NULL);
    if(result||calls!=101||refreshes!=200||pump()||ioa_ui_call(operation,&local))abort();
    ioa_ui_stop();ioa_ui_start();
    if(!ioa_ui_call(operation,&local)||local!=2)abort();
    ioa_ui_stop();
    puts("PASS native renderer queue: actual worker/UI ownership, 100 synchronous results, cancellation and restart");
}
'''
bridge=(ROOT/'app/jni/clutter_main.c').read_text()
a=bridge.index('/* Traverse the actor list only')
b=bridge.index('\n\nvoid ioa_showsplash',a)
stubs=r'''typedef struct ActorData ActorData;
struct ActorData {ActorData *next; void *actor;};
static ActorData third={NULL,(void*)2},second={&third,NULL},first={&second,(void*)1};
static ActorData *actor_data_root=&first;


/* Observe the complete real refresh traversal on its owning thread. */
static void ioa_render_clutter_ext_win_repaint(void *actor)
{
    if(!pthread_equal(pthread_self(),owner)||actor==NULL)abort();
    refreshes++;
}
'''
SOURCE=SOURCE.replace('/* ACTUAL_REFRESH */',stubs+bridge[a:b])
with tempfile.TemporaryDirectory(prefix='ui-thread-' ,dir=ROOT) as directory:
    p=Path(directory);(p/'test.c').write_text(SOURCE)
    subprocess.run(['cc','-std=gnu99','-O2','-Wall','-Wextra','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread','-I'+str(ROOT/'app/jni'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,timeout=15)
