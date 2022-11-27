#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#define TOTAL_NUMBER_OF_STEPS 200000000
#define ERROR -1
#define NO_ERROR 0

typedef struct threadParameters {
    long number_of_threads;
    long long index;
    double partial_sum;
} threadParameters;

void print_error(const char *prefix, int code) {
    char buf[256];
    if (0 != strerror_r(code, buf, sizeof(buf))) {
        strcpy(buf, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buf);
}

void *calculate_partial_sum(void *param) {
    if (NULL == param){
        fprintf(stderr, "calculate_partial_sum: invalid param\n");
        return NULL;
    }
    struct threadParameters *data = (struct threadParameters *)param;
    if (NULL == data) {
        fprintf(stderr, "calculate_partial_sum: invalid param\n");
        return data;
    }

    double partial_sum = 0.0;
    long long index = data->index;
    long number_of_threads = data->number_of_threads;

    for (long long i = index; i <TOTAL_NUMBER_OF_STEPS; i+= number_of_threads) {
        partial_sum += 1.0 / (i * 4.0 + 1.0);
        partial_sum -= 1.0 / (i * 4.0 + 3.0);
    }

    data->partial_sum = partial_sum;
    printf("Thread %lld finished, partial sum %.16f\n", index, data->partial_sum);
    return data;
}

long convert_number_from_string(char *string, long *out) {
    if (NULL == string || NULL == out) {
        fprintf(stderr, "convert_number_from_string : string or out was NULL\n");
        return ERROR;
    }

    errno = 0;
    char *endptr = "";
    *out = strtol(string, &endptr, 10);

    if (NO_ERROR != errno) {
        perror("Can't convert given number");
        return ERROR;
    }
    if (NO_ERROR != strcmp(endptr, "")) {
        fprintf(stderr, "Number contains invalid symbols\n");
        return ERROR;
    }
    return 0;
}

long create_threads(pthread_t *threads, struct threadParameters *data, long number_of_threads) {
    if(number_of_threads < 1 || NULL == data || NULL == threads){
        fprintf(stderr, "create_threads: invalid parameters\n");
        return 0;
    }
    int errorCode;
    long num_of_created = 0;

    for (long i = 0; i < number_of_threads; i++) {
        data[i].index = i;
        data[i].number_of_threads = number_of_threads;

        errorCode = pthread_create(&threads[i], NULL, calculate_partial_sum, &data[i]);
        if (NO_ERROR != errorCode){
            print_error("Unable to create thread", errorCode);
            break;
        }

        num_of_created++;
    }
    return num_of_created;
}

int join_threads(pthread_t *threads, long number_of_threads, double *pi) {
    if(number_of_threads < 1 || NULL == pi || NULL == threads){
        fprintf(stderr, "join_threads: invalid parameters\n");
        return 0;
    }

    int errorCode;
    int return_value = 0;
    double sum = 0.0;

    for (long i = 0; i < number_of_threads; i++) {
        struct threadParameters *res = NULL;

        errorCode = pthread_join(threads[i], (void **)&res);
        if (0 != errorCode) {
            print_error("Unable to join thread", errorCode);
            return_value = -1;
            continue;
        }

        if (NULL == res) {
            fprintf(stderr, "Thread returned NULL\n");
            return_value = -1;
            continue;
        }

        sum += res->partial_sum;
    }
    (*pi) = sum * 4;

    return return_value;
}

int main(int argc, char **argv) {
    if (2 != argc) {
        printf("Usage: %s threads_num\n", argv[0]);
        return 0;
    }

    long number_of_threads;
    if (-1 == convert_number_from_string(argv[1], &number_of_threads)) {
        return EXIT_FAILURE;
    }

    if (number_of_threads < 1) {
        fprintf(stderr, "Number of threads must be positive number\n");
        return EXIT_FAILURE;
    }

    pthread_t threads[number_of_threads];
    struct threadParameters data[number_of_threads];

    long num_of_created = create_threads(threads, data, number_of_threads);
    if (num_of_created < number_of_threads) {
        fprintf(stderr, "Created %ld out of %ld threads!\n", num_of_created, number_of_threads);
    }

    double pi = 0.0;
    if (0 != join_threads(threads, num_of_created, &pi)){
        fprintf(stderr, "Couldn't calculate PI!\n");
        pthread_exit(NULL);
    }

    printf("pi = %.16f\n", pi);
    return EXIT_SUCCESS;
}
