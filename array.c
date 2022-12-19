#include <stdlib.h>
#include <string.h>
#include "array.h"

int array_init(array *s)
{
    s -> front = 0;
    s -> back = -1;

    pthread_mutex_init(&(s -> mutex), NULL);
    sem_init(&(s -> empty), 0, ARRAY_SIZE);
    sem_init(&(s -> full), 0, 0);

    return 0;
}

int array_put (array *s, char *hostname)
{
    //Block when array is full
    sem_wait(&(s -> empty));
    
    //Ensure current thread is only one accessing array
    pthread_mutex_lock(&(s -> mutex));

    s -> back++;

    //Check for overlap and circle back around
    s -> back %= ARRAY_SIZE;

    strcpy(s -> shr_array[s -> back], hostname);

    pthread_mutex_unlock(&(s -> mutex));

    sem_post(&(s -> full));

    return 0;
}

int array_get (array *s, char **hostname)
{
    pthread_mutex_lock(&(s -> mutex));

    //Return fail exit status if it tries to read prematurely
    if (s -> back == -1)
    {
        pthread_mutex_unlock(&(s -> mutex));
        return -1;
    }

    pthread_mutex_unlock(&(s -> mutex));

    //Block when array is empty  
    sem_wait(&(s -> full));

    //Ensure current thread is only one accessing array
    pthread_mutex_lock(&(s -> mutex));

    strcpy(*hostname, s -> shr_array[s -> front]);

    s -> front++;

    //Check for overlap
    s -> front %= ARRAY_SIZE;
    
    pthread_mutex_unlock(&(s -> mutex));

    sem_post(&(s -> empty));

    return 0;
}

void array_free(array *s)
{
    pthread_mutex_destroy(&(s -> mutex));
    sem_destroy(&(s -> empty));
    sem_destroy(&(s -> full));
}