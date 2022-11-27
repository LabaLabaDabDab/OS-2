#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define NO_ERROR 0
#define ERROR -1

#define A_DELAY 1
#define B_DELAY 2
#define C_DELAY 3

#define STOP 1

volatile int executionStatus = 0;

sem_t a_detail_sem;
sem_t b_detail_sem;
sem_t c_detail_sem;
sem_t module_sem;
sem_t widget_sem;

pthread_t a_thread;
pthread_t b_thread;
pthread_t c_thread;
pthread_t module_thread;
pthread_t widget_thread;

int count_a = 0;
int count_b = 0;
int count_c = 0;

int count_module = 0;
int count_widget = 0;

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

void *create_module(){
    while(STOP != executionStatus){
        wait_semaphore(&a_detail_sem);
        wait_semaphore(&b_detail_sem);
        post_semaphore(&module_sem);
        count_module++;
    }
}

void *create_widget(){
    while(STOP != executionStatus){
        wait_semaphore(&c_detail_sem);
        wait_semaphore(&module_sem);
        post_semaphore(&widget_sem);
        count_widget++;
    }
}

void *create_detail_A (){
    while(STOP != executionStatus){
        sleep(A_DELAY);
        post_semaphore(&a_detail_sem);
        count_a++;
    }
}

void *create_detail_B (){
    while(STOP != executionStatus){
        sleep(B_DELAY);
        post_semaphore(&b_detail_sem);
        count_b++;
    }
}

void *create_detail_C (){
    while(STOP != executionStatus){
        sleep(C_DELAY);
        post_semaphore(&c_detail_sem);
        count_c++;
    }
}

void destroy_semaphores(){
    sem_destroy(&a_detail_sem);
    sem_destroy(&b_detail_sem);
    sem_destroy(&c_detail_sem);
    sem_destroy(&module_sem);
    sem_destroy(&widget_sem);
}

void print_stats() {
    fprintf(stdout, "Stopped!\n[%d Widgets, %d Modules, %d details A, %d details B, %d details C]\n", count_widget, count_module, count_a, count_b, count_c);
}

int initialize_semaphores() {
    int errorCode = sem_init(&a_detail_sem, 0, 0);
    if (NO_ERROR != errorCode){
        print_error("Unable to init semaphore: a_detail_sem", errorCode);
        return ERROR;
    }
    errorCode = sem_init(&b_detail_sem, 0, 0);
    if (NO_ERROR != errorCode){
        print_error("Unable to init semaphore: b_detail_sem", errorCode);
        return ERROR;
    }
    errorCode = sem_init(&c_detail_sem, 0, 0);
    if (NO_ERROR != errorCode){
        print_error("Unable to init semaphore: c_detail_sem", errorCode);
        return ERROR;
    }
    errorCode = sem_init(&module_sem, 0, 0);
    if (NO_ERROR != errorCode){
        print_error("Unable to init semaphore: module_sem", errorCode);
        return ERROR;
    }
    errorCode = sem_init(&widget_sem, 0, 0);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init semaphore: widget_sem", errorCode);
        return ERROR;
    }
    return NO_ERROR;
}

int initialize_threads(){
    int errorCode;
    errorCode = pthread_create(&a_thread, NULL, create_detail_A, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create a_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_create(&b_thread, NULL, create_detail_B, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create b_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_create(&c_thread, NULL, create_detail_C, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create c_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_create(&module_thread, NULL, create_module, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create module_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_create(&widget_thread, NULL, create_widget, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to create widget_thread", errorCode);
        return ERROR;
    }
    return NO_ERROR;
}

int joining_threads(){
    int errorCode;
    errorCode = pthread_join(a_thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join a_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_join(b_thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join b_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_join(c_thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join c_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_join(module_thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join module_thread", errorCode);
        return ERROR;
    }
    errorCode = pthread_join(widget_thread, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to join widget_thread", errorCode);
        return ERROR;
    }
    return NO_ERROR;
}

void stopExecution() {
    executionStatus = STOP;
}

int main() {
    signal(SIGINT, stopExecution);

    if (NO_ERROR != initialize_semaphores()){
        return EXIT_FAILURE;
    }
    if (NO_ERROR != initialize_threads()){
        destroy_semaphores();
        return EXIT_FAILURE;
    }

    if (NO_ERROR != joining_threads()){
        destroy_semaphores();
        return EXIT_FAILURE;
    }

    print_stats();

    destroy_semaphores();
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}

