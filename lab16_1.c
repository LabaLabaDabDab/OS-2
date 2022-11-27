#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUMBER_OF_LINES 10

#define NO_ERROR 0
#define ERROR -1

sem_t *sem1;
sem_t *sem2;

void print_error(const char *prefix, int code) {
    char buffer[256];
    if (NO_ERROR != strerror_r(code, buffer, sizeof(buffer))) {
        strcpy(buffer, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buffer);
}

void shutdown() {
    sem_close(sem1);
    sem_close(sem2);
    sem_unlink("/Parent");
    sem_unlink("/Child");
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

int main() {
    sem1 = sem_open("/Parent", O_CREAT, 0777, 1);
    if (SEM_FAILED == sem1) {
        perror("Unable to init sem1");
        sem_close(sem1);
        return EXIT_FAILURE;
    }
    sem2 = sem_open("/Child", O_CREAT, 0777, 0);
    if (SEM_FAILED == sem2) {
        perror("Unable to init sem2");
        shutdown();
        return EXIT_FAILURE;
    }
    for (int i = 0; i < NUMBER_OF_LINES; i++) {
        if (NO_ERROR != wait_semaphore(sem1)) {
            shutdown();
            return EXIT_FAILURE;
        }
        if (NO_ERROR == write(STDOUT_FILENO, "Parent\n", 7)) {
            perror("write error");
            shutdown();
            return EXIT_FAILURE;
        }
        if (NO_ERROR != post_semaphore(sem2)) {
            shutdown();
            return EXIT_FAILURE;
        }
    }
    shutdown();
    return EXIT_SUCCESS;
}
