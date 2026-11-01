#ifndef IOA_UI_QUEUE_H
#define IOA_UI_QUEUE_H // Synchronous native renderer calls, dispatched exclusively by the Clutter main loop.
#include <pthread.h>

typedef struct IoaUiJob IoaUiJob;

struct IoaUiJob {
    IoaUiJob *next; // Pending-list link, protected by ioa_ui_mutex.
    void (*function)(void*); // Renderer operation that must execute on the UI owner.
    void *argument; // Borrowed until the synchronous submitter is released.
    pthread_cond_t ready; // Wakes the submitter after execution or shutdown cancellation.
    guint source; // GLib idle source, removed before a cancelled job leaves its stack.
    int done, accepted; // Completion state protected by the same pending-list mutex.
};

static pthread_mutex_t ioa_ui_mutex = PTHREAD_MUTEX_INITIALIZER; // Serializes job lifetime and shutdown.
static pthread_t ioa_ui_owner; // Only this thread may invoke the renderer operation.
static IoaUiJob *ioa_ui_pending; // Stack-owned jobs remain linked until dispatch or cancellation.
static int ioa_ui_active; // Closed before the native event loop and GLES context retire.


/* Remove and execute one pending job on the UI thread, then publish its synchronous result. */
static gboolean
ioa_ui_dispatch(gpointer argument)
{
    IoaUiJob *job = argument, **link;

    pthread_mutex_lock(&ioa_ui_mutex);
    for(link = &ioa_ui_pending; *link != NULL && *link != job; link = &(*link)->next)
        ;
    if(*link == NULL){
        pthread_mutex_unlock(&ioa_ui_mutex);
        return FALSE;
    }
    *link = job->next;
    pthread_mutex_unlock(&ioa_ui_mutex);
    job->function(job->argument);
    pthread_mutex_lock(&ioa_ui_mutex);
    job->accepted = job->done = 1;
    pthread_cond_signal(&job->ready);
    pthread_mutex_unlock(&ioa_ui_mutex);
    return FALSE;
}


/* Bind the default Clutter context to its actual native owner before starting VM workers. */
static void
ioa_ui_start(void)
{
    pthread_mutex_lock(&ioa_ui_mutex);
    ioa_ui_owner = pthread_self();
    ioa_ui_active = 1;
    pthread_mutex_unlock(&ioa_ui_mutex);
}


/* Reject later calls and remove pending sources before releasing their borrowed stack arguments. */
static void
ioa_ui_stop(void)
{
    IoaUiJob *job, *next;

    pthread_mutex_lock(&ioa_ui_mutex);
    ioa_ui_active = 0;
    for(job = ioa_ui_pending; job != NULL; job = next){
        next = job->next;
        g_source_remove(job->source);
        job->done = 1;
        pthread_cond_signal(&job->ready);
    }
    ioa_ui_pending = NULL;
    pthread_mutex_unlock(&ioa_ui_mutex);
}


/* Never use g_main_context_invoke: it may execute immediately on the submitting worker. */
static int
ioa_ui_call(void (*function)(void*), void *argument)
{
    IoaUiJob job = {0};
    int accepted;

    pthread_mutex_lock(&ioa_ui_mutex);
    if(!ioa_ui_active){
        pthread_mutex_unlock(&ioa_ui_mutex);
        return 0;
    }
    if(pthread_equal(pthread_self(), ioa_ui_owner)){
        pthread_mutex_unlock(&ioa_ui_mutex);
        function(argument);
        return 1;
    }
    if(pthread_cond_init(&job.ready, NULL) != 0){
        pthread_mutex_unlock(&ioa_ui_mutex);
        return 0;
    }
    job.function = function; job.argument = argument;
    job.next = ioa_ui_pending; ioa_ui_pending = &job;
    job.source = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, ioa_ui_dispatch, &job, NULL);
    if(job.source == 0){
        ioa_ui_pending = job.next;
        pthread_mutex_unlock(&ioa_ui_mutex);
        pthread_cond_destroy(&job.ready);
        return 0;
    }
    while(!job.done)
        pthread_cond_wait(&job.ready, &ioa_ui_mutex);
    accepted = job.accepted;
    pthread_mutex_unlock(&ioa_ui_mutex);
    pthread_cond_destroy(&job.ready);
    return accepted;
}
#endif
