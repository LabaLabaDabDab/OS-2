#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define FOOD 50
#define DELAY 30000
#define NUM_OF_PHILO 5
#define NO_ERROR 0
#define ERROR -1

#define LOCKED 0
#define NOT_LOCKED (-1)

pthread_t threads[NUM_OF_PHILO];
pthread_mutex_t food_mutex;
pthread_mutex_t forks_mutex[NUM_OF_PHILO];
pthread_mutex_t entry_point_mutex;
pthread_cond_t entry_point_cond;

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

int try_lock_mutex(pthread_mutex_t *mutex) {
    if (NULL == mutex){
        fprintf(stderr, "try_lock_mutex: mutex was NULL\n");
        return ERROR;
    }
    int error_code = pthread_mutex_trylock(mutex);
    if (NO_ERROR != error_code) {
        if (error_code == EBUSY) {
            return NOT_LOCKED;
        }
        print_error("Unable to try-lock mutex", error_code);
        return error_code;
    }
    return LOCKED;
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

int broadcast_cond(pthread_cond_t *cond) {
    if (NULL == cond){
        fprintf(stderr, "broadcast_cond: cond was NULL\n");
        return ERROR;
    }
    int errorCode = pthread_cond_broadcast(cond);
    if (NO_ERROR != errorCode) {
        print_error("Unable to broadcast cond variable", errorCode);
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

void pick_forks_up(int id, int left_fork, char right_fork) {
    int lock1, lock2;
    lock_mutex(&entry_point_mutex);
    if(id % 2 == 0){
        while(1){
            lock1 = try_lock_mutex(&forks_mutex[right_fork]);
            if (LOCKED == lock1){
                lock2 = try_lock_mutex(&forks_mutex[left_fork]);
                if (LOCKED == lock2){
                    break;
                }
                unlock_mutex(&forks_mutex[right_fork]);
            }
            wait_cond(&entry_point_cond, &entry_point_mutex);
        }
    }
    else{
        while(1){
            lock1 = try_lock_mutex(&forks_mutex[left_fork]);
            if (LOCKED == lock1){
                lock2 = try_lock_mutex(&forks_mutex[right_fork]);
                if (LOCKED == lock2){
                    break;
                }
                unlock_mutex(&forks_mutex[left_fork]);
            }
            wait_cond(&entry_point_cond, &entry_point_mutex);
        }
    }
    unlock_mutex(&entry_point_mutex);
    printf("Philosopher %d: got %d fork %d\n", id, left_fork, right_fork);
}

void put_forks_down(int left_fork, int right_fork) {
    //lock_mutex(&entry_point_mutex);

    unlock_mutex(&forks_mutex[left_fork]);
    unlock_mutex(&forks_mutex[right_fork]);

    broadcast_cond(&entry_point_cond);
    //unlock_mutex(&entry_point_mutex);
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
        total++;
        printf("Philosopher %d: gets food %d.\n", id, food);

        pick_forks_up(id, left_fork, right_fork);

        printf("Philosopher %d: eats.\n", id);
        usleep(DELAY * (FOOD - food + 1));

        put_forks_down(left_fork, right_fork);

        //sched_yield();
    }

    printf("Philosopher %d is done eating. Ate %d out of %d portions\n", id, total, FOOD);
    return NULL;
}

void cleanup() {
    pthread_mutex_destroy(&food_mutex);
    pthread_mutex_destroy(&entry_point_mutex);
    pthread_cond_destroy(&entry_point_cond);
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
    errorCode = pthread_mutex_init(&entry_point_mutex, NULL);
    if (NO_ERROR != errorCode){
        print_error("Unable to init mutex entry_point_mutex", errorCode);
        pthread_mutex_destroy(&food_mutex);
        return ERROR;
    }
    errorCode = pthread_cond_init(&entry_point_cond, NULL);
    if (NO_ERROR != errorCode) {
        print_error("Unable to init cond", errorCode);
        pthread_mutex_destroy(&food_mutex);
        pthread_mutex_destroy(&entry_point_mutex);
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
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}

