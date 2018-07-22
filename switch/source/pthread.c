#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <pthread.h>

#define STACKSIZE (4 * 1024)

void *(*start_routine_tmp)(void *);
void thread_entry(void *arg) {
    start_routine_tmp(arg);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
    s32 priority = 0x2B;
    Thread *temp_thread = malloc(sizeof(Thread));
    
    start_routine_tmp = start_routine;
    
    int rc = threadCreate(temp_thread, thread_entry, arg, STACKSIZE, priority, -2);
    if (R_FAILED(rc)) {
        printf("Create Thread failed\n");
        return -1;
    }
    
    rc = threadStart(temp_thread);
    if (R_FAILED(rc)) {
        printf("Start Thread failed\n");
        return -1;
    }
    
    pthread_t tmp;
    tmp = temp_thread->handle;
    *thread = tmp;
    return 0;
}

int pthread_join(pthread_t thread, void **retval) {
    printf("%s currently not supported, exit...\n", __FUNCTION__);
    exit(1);
    return 0;
}

void pthread_exit(void *retval) {
    printf("%s currently not supported, exit...\n", __FUNCTION__);
    exit(1);
}
