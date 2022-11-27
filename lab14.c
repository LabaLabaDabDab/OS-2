#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

#define NUMBER_OF_LINES 10

#define NUMBER_OF_SEMAFOR 2
#define SECOND_SEMAPHORE 1
#define FIRST_SEMAPHORE 0
#define NO_ERROR 0
#define ERROR -1

typedef struct pthreadParameters {
    sem_t semaphores[NUMBER_OF_SEMAFOR];
}pthreadParameters;

void print_error(const char *prefix, int code) {
    char buffer[256];
    if (NO_ERROR != strerror_r(code, buffer, sizeof(buffer))) {
        strcpy(buffer, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buffer);
}

int wait_semaphore(sem_t *sem) {
    if (NULL == sem){
        fprintf(stderr, "wait_semaphore: sem was NULL\n");
        return ERROR;
    }
    int errorCode = sem_wait(sem);
    if (ERROR == errorCode) {
        print_error("Unable to wait semaphore", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

int post_semaphore(sem_t *sem) {
    if (NULL == sem){
        fprintf(stderr, "post_semaphore: sem was NULL\n");
        return ERROR;
    }
    int errorCode = sem_post(sem);
    if (ERROR == errorCode) {
        print_error("Unable to wait semaphore", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

void *print_messages(struct pthreadParameters *parameters, int first_sem, int second_sem, const char *message) {
    if (NULL == message) {
        fprintf(stderr, "print_messages: message was null");
        return NULL;
    }
    size_t msg_length = strlen(message);
    printf("%d %d\n", first_sem, second_sem);

    for (int i = 0; i < NUMBER_OF_LINES; i++) {
        if (NO_ERROR != wait_semaphore(&parameters->semaphores[first_sem])){
            return NULL;
        }
        if (NO_ERROR == write(STDOUT_FILENO, message, msg_length)){
            perror("write error");
            return NULL;
        }
        if (NO_ERROR != post_semaphore(&parameters->semaphores[second_sem])){
            return NULL;
        }
    }
    return NULL;
}

void *second_print(void *parameters) {
    print_messages(parameters, SECOND_SEMAPHORE, FIRST_SEMAPHORE, "Child\n");
    return NULL;
}

int main() {
    struct pthreadParameters parameters;

    int errorCode = sem_init(&parameters.semaphores[FIRST_SEMAPHORE], 0, 1);
    if (ERROR == errorCode) {
        perror("Unable to init semaphore");
        return EXIT_FAILURE;
    }

    errorCode = sem_init(&parameters.semaphores[SECOND_SEMAPHORE], 0, 0);
    if (ERROR == errorCode) {
        perror("Unable to init semaphore");
        sem_destroy(&parameters.semaphores[FIRST_SEMAPHORE]);
        return EXIT_FAILURE;
    }

    pthread_t thread;
    errorCode = pthread_create(&thread, NULL, second_print, &parameters);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create thread", errorCode);
        sem_destroy(&parameters.semaphores[FIRST_SEMAPHORE]);
        sem_destroy(&parameters.semaphores[SECOND_SEMAPHORE]);
        return EXIT_FAILURE;
    }

    print_messages(&parameters, FIRST_SEMAPHORE, SECOND_SEMAPHORE, "Parent\n");

    errorCode = pthread_join(thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join thread", errorCode);
        return EXIT_FAILURE;
    }

    sem_destroy(&parameters.semaphores[FIRST_SEMAPHORE]);
    sem_destroy(&parameters.semaphores[SECOND_SEMAPHORE]);
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}
