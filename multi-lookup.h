#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include "array.h"
#include "util.h"

#define MAX_INPUT_FILES 100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS 10
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

#define MIN_ARG_COUNT 6

//Struct used to hold arguments for threads
typedef struct Args
{
    array *hostnames_array;

    FILE *request_log;
    FILE *resolve_log;

    pthread_mutex_t *std_lock;
    pthread_mutex_t *arg_lock;
    pthread_mutex_t *request_log_lock;
    pthread_mutex_t *resolve_log_lock;

    int argc;
    char **argv;
    int *argv_pos;
    int *isFinished;
    int *linesRequested;
    int *linesResolved;

} args;

void *request(void *param);
void *resolve(void *param);

#endif