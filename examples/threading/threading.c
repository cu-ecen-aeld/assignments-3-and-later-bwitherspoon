#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_args = (struct thread_data *)thread_param;
    int ret;

    ret = usleep(1000 * thread_args->wait_to_obtain_ms);
    if (ret != 0) {
        perror("usleep");
        thread_args->thread_complete_success = false;
        return thread_param;
    }

    ret = pthread_mutex_lock(thread_args->mutex);
    if (ret != 0) {
        errno = ret;
        perror("pthread_mutex_lock");
        thread_args->thread_complete_success = false;
        return thread_param;
    }

    ret = usleep(1000 * thread_args->wait_to_release_ms);
    if (ret != 0) {
        perror("usleep");
        thread_args->thread_complete_success = false;
        return thread_param;
    }

    ret = pthread_mutex_unlock(thread_args->mutex);
    if (ret != 0) {
        errno = ret;
        perror("pthread_mutex_unlock");
        thread_args->thread_complete_success = false;
        return thread_param;
    }

    thread_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    int ret;
    struct thread_data *thread_args;

    thread_args = calloc(1, sizeof(struct thread_data));
    if (thread_args == NULL) {
        perror("calloc");
        return false;
    }

    thread_args->mutex = mutex;
    thread_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_args->wait_to_release_ms = wait_to_release_ms;

    ret = pthread_create(thread, NULL, threadfunc, thread_args);
    if (ret != 0) {
        errno = ret;
        perror("pthread_create");
        free(thread_args);
        return false;
    }

    return true;
}

