#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define NUMBER_OF_LINES 10
#define NUMBER_OF_MUTEXES 3
#define ZERO_MUTEX 0
#define SECOND_MUTEX 2
#define STEP 1
#define SLEEP_TIME 500000
#define NO_ERROR 0
#define ERROR -1
#define PTHREAD_SUCCESS 0

typedef struct pthreadParameters {
    pthread_mutex_t mutexes[NUMBER_OF_MUTEXES];
}pthreadParameters;

void print_error(const char *prefix, int code) {
    char buffer[256];
    if (NO_ERROR != strerror_r(code, buffer, sizeof(buffer))) {
        strcpy(buffer, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buffer);
}

int lock_mutex(pthread_mutex_t *mutex) {
    if (NULL == mutex){
        fprintf(stderr, "lock_mutex: mutex was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_mutex_lock(mutex);
    if (NO_ERROR != errorCode) {
        print_error("Unable to lock mutex", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

int unlock_mutex(pthread_mutex_t *mutex) {
    if (NULL == mutex){
        fprintf(stderr, "lock_mutex: mutex was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_mutex_unlock(mutex);
    if (NO_ERROR != errorCode) {
        print_error("Unable to unlock mutex", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

void* print_lines(struct pthreadParameters *parameters, int currentMutex, char *string){
    if (NULL == parameters){
        fprintf(stderr, "Param is NULL\n");
        return NULL;
    }
    size_t sizeString = strlen(string);
    for (int i = 0; i < NUMBER_OF_MUTEXES * NUMBER_OF_LINES; ++i) {
        if (ZERO_MUTEX == currentMutex){
            write(STDOUT_FILENO, string, sizeString);
        }
        int nextMutex = (currentMutex + STEP) % NUMBER_OF_MUTEXES;
        lock_mutex(&parameters->mutexes[nextMutex]);
        unlock_mutex(&parameters->mutexes[currentMutex]);
        currentMutex = nextMutex;
    }
    return NULL;
}

void *second_print(void *param) {
    if (NULL == param){
        fprintf(stderr, "Param is NULL\n");
        return NULL;
    }
    struct pthreadParameters *parameters =  (pthreadParameters *) param;
    lock_mutex(&parameters->mutexes[SECOND_MUTEX]);
    print_lines(parameters, SECOND_MUTEX, "Child\n");
    return NULL;
}

void destroy_mutexes(struct pthreadParameters *parameters, int count) {
    for (int i = 0; i < count; i++) {
        pthread_mutex_destroy(&parameters->mutexes[i]);
    }
}

int init_parameters(struct pthreadParameters *parameters) {
    pthread_mutexattr_t mutex_attrs;
    int errorCode = pthread_mutexattr_init(&mutex_attrs);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init mutex attrs", errorCode);
        return ERROR;
    }

    errorCode = pthread_mutexattr_settype(&mutex_attrs, PTHREAD_MUTEX_ERRORCHECK);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init mutex attrs type", errorCode);
        pthread_mutexattr_destroy(&mutex_attrs);
        return ERROR;
    }

    for (int i = 0; i < NUMBER_OF_MUTEXES; i++) {
        errorCode = pthread_mutex_init(&parameters->mutexes[i], &mutex_attrs);
        if (NO_ERROR != errorCode) {
            pthread_mutexattr_destroy(&mutex_attrs);
            destroy_mutexes(parameters, i);
            return ERROR;
        }
    }
    pthread_mutexattr_destroy(&mutex_attrs);
    return NO_ERROR;
}

int main() {
    struct pthreadParameters parameters;
    int errorCode = init_parameters(&parameters);
    if (NO_ERROR != errorCode) {
        return EXIT_FAILURE;
    }

    lock_mutex(&parameters.mutexes[ZERO_MUTEX]);

    pthread_t thread;
    errorCode = pthread_create(&thread, NULL, second_print, &parameters);
    if (PTHREAD_SUCCESS != errorCode) {
        print_error("Unable to create thread", errorCode);
        unlock_mutex(&parameters.mutexes[ZERO_MUTEX]);
        destroy_mutexes(&parameters, NUMBER_OF_MUTEXES);
        return EXIT_FAILURE;
    }

    while (1) {
        errorCode = pthread_mutex_trylock(&parameters.mutexes[SECOND_MUTEX]);
        if (EBUSY == errorCode) {
            break;
        }
        if (NO_ERROR != errorCode ){
            print_error("Error trylocking mutex", errorCode);
            return EXIT_FAILURE;
        }
        errorCode = pthread_mutex_unlock(&parameters.mutexes[SECOND_MUTEX]);
        if (NO_ERROR != errorCode){
            print_error("Error unlocking mutex", errorCode);
            return EXIT_FAILURE;
        }
        usleep(SLEEP_TIME);
    }

    print_lines(&parameters, ZERO_MUTEX, "Parent\n");
    unlock_mutex(&parameters.mutexes[ZERO_MUTEX]);
    destroy_mutexes(&parameters, NUMBER_OF_MUTEXES);
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}
