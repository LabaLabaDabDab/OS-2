#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUMBER_OF_LINES 10

#define MAIN_THREAD 0
#define CHILD_THREAD 1
#define NO_ERROR 0
#define ERROR -1

typedef struct pthreadParameters {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int cur_printing_thread;
}pthreadParameters;

void print_error(const char *prefix, int code) {
    char buffer[256];
    if (0 != strerror_r(code, buffer, sizeof(buffer))) {
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
        fprintf(stderr, "unlock_mutex: mutex was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_mutex_unlock(mutex);
    if (NO_ERROR != errorCode) {
        print_error("Unable to unlock mutex", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

int wait_cond(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (NULL == mutex || NULL == cond){
        fprintf(stderr, "wait_cond: mutex or cond was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_cond_wait(cond, mutex);
    if (NO_ERROR != errorCode) {
        print_error("Unable to wait cond variable", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

int signal_cond(pthread_cond_t *cond) {
    if (NULL == cond){
        fprintf(stderr, "signal_cond: cond was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_cond_signal(cond);
    if (NO_ERROR != errorCode) {
        print_error("Unable to signal cond", errorCode);
        return errorCode;
    }
    return NO_ERROR;
}

void *print_messages(struct pthreadParameters *parameters, const char *message, int calling_thread) {
    if (NULL == message) {
        fprintf(stderr, "print_messages: invalid parameter\n");
        return NULL;
    }
    size_t msg_length = strlen(message);

    for (int i = 0; i < NUMBER_OF_LINES; i++) {
    	if (NO_ERROR != lock_mutex(&parameters->mutex)){
		return NULL;
	}
        while (calling_thread != parameters->cur_printing_thread) {
            if (NO_ERROR != wait_cond(&parameters->cond, &parameters->mutex)){
                return NULL;
            }
        }

        if (ERROR == write(STDOUT_FILENO, message, msg_length)){
            perror("write error");
            return NULL;
        }
        parameters->cur_printing_thread = (parameters->cur_printing_thread == MAIN_THREAD) ? CHILD_THREAD : MAIN_THREAD;
        if (NO_ERROR != signal_cond(&parameters->cond)){
            return NULL;
        }
        if (NO_ERROR != unlock_mutex(&parameters->mutex)){
        return NULL;
    	}
    }

}

void *second_print(void *param) {
    if (NULL == param){
        fprintf(stderr, "Param is NULL\n");
        return NULL;
    }
    print_messages(param, "Child\n", CHILD_THREAD);
    return NULL;
}

int init(struct pthreadParameters *parameters) {
    int errorCode = pthread_mutex_init(&parameters->mutex, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init mutex", errorCode);
        return ERROR;
    }

    errorCode = pthread_cond_init(&parameters->cond, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init cond", errorCode);
        pthread_mutex_destroy(&parameters->mutex);
        return ERROR;
    }
    parameters->cur_printing_thread = MAIN_THREAD;
    return NO_ERROR;
}

void cleanup(struct pthreadParameters *parameters) {
    pthread_mutex_destroy(&parameters->mutex);
    pthread_cond_destroy(&parameters->cond);
}

int main() {
    struct pthreadParameters parameters;
    if (NO_ERROR != init(&parameters)) {
        return EXIT_FAILURE;
    }

    pthread_t thread;
    int errorCode = pthread_create(&thread, NULL, second_print, &parameters);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create thread", errorCode);
        cleanup(&parameters);
        return EXIT_FAILURE;
    }

    print_messages(&parameters,"Parent\n", MAIN_THREAD);

    cleanup(&parameters);
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}
