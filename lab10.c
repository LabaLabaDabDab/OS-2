#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>

#define FOOD 50
#define DELAY 30000
#define NUM_OF_PHILO 5
#define NO_ERROR 0
#define ERROR -1

pthread_t threads[NUM_OF_PHILO];
pthread_mutex_t food_mutex;
pthread_mutex_t forks_mutex[NUM_OF_PHILO];

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

int get_food(int id) {
    static int total_food = FOOD;
    int my_food;

    lock_mutex(&food_mutex);
    my_food = total_food;
    if (total_food > 0) {
        total_food--;
    }
    unlock_mutex(&food_mutex);

    return my_food;
}

void pick_fork_up(int phil, int fork, char *hand) {
    lock_mutex(&forks_mutex[fork]);
    printf("Philosopher %d: got %s fork %d\n", phil, hand, fork);
}

void put_forks_down(int left_fork, int right_fork) {
    unlock_mutex(&forks_mutex[left_fork]);
    unlock_mutex(&forks_mutex[right_fork]);
}

void *philosopher(void *param) {

    int id = (int*)param;
    int right_fork = id;
    int left_fork = id + 1;

    if (left_fork == NUM_OF_PHILO) {
        left_fork = right_fork;
        right_fork = 0;
    }

    printf("Philosopher %d sitting down to dinner.\n", id);

    int food;
    int total = 0;
    while ((food = get_food(id)) > 0) {
        if(id % 2 == 0){
            total++;
            printf("Philosopher %d: gets food %d.\n", id, food);


            pick_fork_up(id, left_fork, "left");
            sleep(5);
            pick_fork_up(id, right_fork, "right");


            printf("Philosopher %d: eats.\n", id);
            usleep(DELAY * (FOOD - food + 1));
        }
        else{
            total++;
            printf("Philosopher %d: gets food %d.\n", id, food);


            pick_fork_up(id, right_fork, "right");
            sleep(5);
            pick_fork_up(id, left_fork, "left");


            printf("Philosopher %d: eats.\n", id);
            usleep(DELAY * (FOOD - food + 1));
        }

        put_forks_down(left_fork, right_fork);
    }

    printf("Philosopher %d is done eating. Ate %d out of %d portions\n", id, total, FOOD);
    return NULL;
}

void cleanup() {
    pthread_mutex_destroy(&food_mutex);
    for (int i = 0; i < NUM_OF_PHILO; i++) {
        pthread_mutex_destroy(&forks_mutex[i]);
    }
}

int init(){
    int errorCode;
    errorCode = pthread_mutex_init(&food_mutex, NULL);
    if (NO_ERROR != errorCode){
        print_error("Unable to init food_mutex", errorCode);
        return ERROR;
    }
    for (int i = 0; i < NUM_OF_PHILO; i++) {
        errorCode = pthread_mutex_init(&forks_mutex[i], NULL);
        if (NO_ERROR != errorCode){
            print_error("Unable to init forks_mutex", errorCode);
            cleanup();
            return ERROR;
        }
    }
    return NO_ERROR;
}

int main() {
    int errorCode = init();
    if (NO_ERROR != errorCode){
        return EXIT_FAILURE;
    }
    for (int i = 0; i < NUM_OF_PHILO; i++) {
        errorCode = pthread_create(&threads[i], NULL, philosopher, (void *)i);
        if (NO_ERROR != errorCode) {
            print_error("Unable to create thread", errorCode);
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < NUM_OF_PHILO; i++) {
        errorCode = pthread_join(threads[i], NULL);
        if (NO_ERROR != errorCode) {
            print_error("Unable to join thread", errorCode);
            return EXIT_FAILURE;
        }
    }
    cleanup();
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}

