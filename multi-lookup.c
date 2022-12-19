#include <stdio.h>
#include <sys/time.h>
#include "multi-lookup.h"

int main (int argc, char **argv)
{
    array hostnames_array;

    pthread_mutex_t std_lock;
    pthread_mutex_t arg_lock;
    pthread_mutex_t request_log_lock;
    pthread_mutex_t resolve_log_lock;

    if (argc < MIN_ARG_COUNT)
    {
        fprintf(stderr, "Failed to run: Missing arguments\n");
        fprintf(stderr, "Usage: ./program_name num_threads num_threads request_log.txt resolve_log.txt input.txt\n");
        return -1;
    }

    int num_req_threads = 0;
    int num_res_threads = 0;

    sscanf(argv[1], "%d", &num_req_threads);
    sscanf(argv[2], "%d", &num_res_threads);

    if (num_req_threads > MAX_REQUESTER_THREADS || num_req_threads <= 0)
    {
        fprintf(stderr, "Failed to run: Arguments out of range\n");
        return -1;
    }
    else if (num_res_threads > MAX_RESOLVER_THREADS || num_res_threads <= 0)
    {
        fprintf(stderr, "Failed to run: Arguments out of range\n");
        return -1;
    }

    //Execution time code courtesy of https://linuxhint.com/gettimeofday_c_language/
    struct timeval start, end;
    gettimeofday(&start, NULL);

    pthread_t requester[num_req_threads];
    pthread_t resolver[num_res_threads];

    FILE *request_log = fopen(argv[3], "w");
    FILE *resolve_log = fopen(argv[4], "w");

    int argv_pos = 5;
    int isFinished = 0;
    int linesRequested = 0;
    int linesResolved = 0;

    //Sets the arguments to be used by the threads
    args args;
    args.hostnames_array = &hostnames_array;

    args.request_log = request_log;
    args.resolve_log = resolve_log;
    
    args.std_lock = &std_lock;
    args.arg_lock = &arg_lock;
    args.request_log_lock = &request_log_lock;
    args.resolve_log_lock = &resolve_log_lock;

    args.argc = argc;
    args.argv = argv;
    args.argv_pos = &argv_pos;
    args.isFinished = &isFinished;
    args.linesRequested = &linesRequested;
    args.linesResolved = &linesResolved;

    //Ensure that all mutexes are created successfully
    if (pthread_mutex_init(&std_lock, NULL) != 0)
    {
        perror("Failed to create mutex std_lock");
        return -1;
    }
    else if (pthread_mutex_init(&arg_lock, NULL) != 0)
    {
        perror("Failed to create mutex arg_lock");
        return -1;
    }
    else if (pthread_mutex_init(&request_log_lock, NULL) != 0)
    {
        perror("Failed to create mutex request_log_lock");
        return -1;
    }
    else if (pthread_mutex_init(&resolve_log_lock, NULL) != 0)
    {
        perror("Failed to create mutex resolve_log_lock");
        return -1;
    }

    array_init(&hostnames_array);

    //Create the specified number of requester threads
    for (int i = 0; i < num_req_threads; i++)
    {
        if (pthread_create(&requester[i], NULL, &request, &args) != 0)
        {
            perror("Failed to create thread");
        }
    }

    //Create the specified number of resolver threads
    for (int i = 0; i < num_res_threads; i++)
    {
        if (pthread_create(&resolver[i], NULL, &resolve, &args) != 0)
        {
            perror("Failed to create thread");
        }
    }

    //Wait for requester threads to complete and then join into main thread
    for (int i = 0; i < num_req_threads; i++)
        pthread_join(requester[i], NULL);

    //Signal resolver threads that requesters are finished
    pthread_mutex_lock(&arg_lock);
    isFinished = 1;
    pthread_mutex_unlock(&arg_lock);

    //Wait for resolver threads to complete and then join into main thread
    for (int i = 0; i < num_res_threads; i++)
        pthread_join(resolver[i], NULL);

    if (fclose(request_log) != 0)
        perror("Failed to close file");

    if (fclose(resolve_log) != 0)
        perror("Failed to close file");

    pthread_mutex_destroy(&std_lock);
    pthread_mutex_destroy(&arg_lock);
    pthread_mutex_destroy(&request_log_lock);
    pthread_mutex_destroy(&resolve_log_lock);

    array_free(&hostnames_array);

    //Execution time code courtesy of https://linuxhint.com/gettimeofday_c_language/
    gettimeofday(&end, NULL);
    float time_spent = (float)((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)) / 1000000;

    printf("Total runtime is %f seconds\n", time_spent);

    return 0;
}

void *request(void *param)
{
    int files_serviced = 0;

    args args = *(struct Args*)param;

    while (1)
    {
        //Check to see if all of the files have been read
        pthread_mutex_lock(args.arg_lock);

        if (*args.argv_pos >= args.argc)
        {
            pthread_mutex_unlock(args.arg_lock);
            break;
        }
        
        char *filename = args.argv[*args.argv_pos];
        (*args.argv_pos)++;
        
        pthread_mutex_unlock(args.arg_lock);

        FILE *file = fopen(filename, "r");

        if (file == NULL)
        {
            pthread_mutex_lock(args.std_lock);
            fprintf(stderr, "Invalid filename %s\n", filename);
            pthread_mutex_unlock(args.std_lock);

            continue;
        }

        files_serviced++;

        char buffer[MAX_NAME_LENGTH];

        //Loop through every line and add hostname to array
        while (fgets(buffer, MAX_NAME_LENGTH, file))
        {
            //Increment the number of lines that have been requested
            pthread_mutex_lock(args.arg_lock);
            (*args.linesRequested)++;
            pthread_mutex_unlock(args.arg_lock);

            char string[MAX_NAME_LENGTH];

            sscanf(buffer, "%s", string);
            array_put(args.hostnames_array, string);

            pthread_mutex_lock(args.request_log_lock);

            if (fprintf(args.request_log, "%s\n", string) < 0)
            {
                perror("Failed to write to file");
            }

            pthread_mutex_unlock(args.request_log_lock);
        } 

        if (fclose(file) != 0)
        {
            perror("Failed to close file");
        }
    }

    pthread_mutex_lock(args.std_lock);
    printf("Thread %x serviced %d files\n", (int)pthread_self(), files_serviced);
    pthread_mutex_unlock(args.std_lock);

    return NULL;
}

void *resolve(void *param)
{
    int names_resolved = 0;

    args args = *(struct Args*)param;

    while(1)
    {
        //Check if thread should terminate
        pthread_mutex_lock(args.arg_lock);

        if (*args.isFinished == 1 && (*args.linesRequested == *args.linesResolved || *args.linesRequested == 0))
        {
            pthread_mutex_unlock(args.arg_lock);
            break;
        }

        pthread_mutex_unlock(args.arg_lock);

        //This needs to be malloc because array_get requires double pointer
        char *name = (char *) malloc(MAX_NAME_LENGTH);
        char ip[MAX_IP_LENGTH];

        //Check for premature gets
        if (array_get(args.hostnames_array, &name) != 0)
        {
            free(name);
            continue;
        }

        names_resolved++;

        pthread_mutex_lock(args.arg_lock);
        (*args.linesResolved)++;
        pthread_mutex_unlock(args.arg_lock);

        if (dnslookup(name, ip, MAX_IP_LENGTH) != 0)
        {
            strcpy(ip, "NOT_RESOLVED");
        }

        pthread_mutex_lock(args.resolve_log_lock);

        if (fprintf(args.resolve_log, "%s, %s\n", name, ip) < 0)
        {
            perror("Failed to write to file");
        }

        pthread_mutex_unlock(args.resolve_log_lock);

        free(name);
    }

    pthread_mutex_lock(args.std_lock);
    printf("Thread %x resolved %d hostnames\n", (int)pthread_self(), names_resolved);
    pthread_mutex_unlock(args.std_lock);

    return NULL;
}