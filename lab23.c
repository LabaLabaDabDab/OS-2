#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define MAX_NUM_OF_LINES 100
#define RATIO 200000
#define BUF_SIZE 4096
#define NO_ERROR 0
#define ERROR -1

pthread_mutex_t mutex;
int all_threads_created = 0;

typedef struct node_t{
    char *string;
    struct node_t *next;
    size_t s_len;
} node_t;

typedef struct list_t{
    node_t *head;
    node_t *tail;
} list_t;

list_t list = (list_t) { .head = NULL, .tail = NULL };

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

int print_list() {
    printf("--Your list--\n");
    node_t *temp = list.head;
    while (NULL != temp) {
        printf("%s", temp->string);
        temp = temp->next;
    }
    printf("--End of list--\n");
    return NO_ERROR;
}

void free_list() {
    node_t *temp = list.head;
    node_t *next = NULL;
    while (NULL != temp) {
        next = temp->next;
        free(temp->string);
        free(temp);
        temp = next;
    }
    pthread_mutex_destroy(&mutex);
}

int list_insert(node_t *node){
    if (NO_ERROR != lock_mutex(&mutex)){
        perror("list_insert: Unable to lock mutex");
        return ERROR;
    }
    if (list.head == NULL){
        list.head = node;
        list.head->next = NULL;
        list.tail = list.head;
    }
    else {
        list.tail->next = node;
        list.tail->next->next = NULL;
        list.tail = list.tail->next;
    }
    if (NO_ERROR != unlock_mutex(&mutex)){
        perror("list_insert: Unable to unlock mutex");
        return ERROR;
    }
    return NO_ERROR;
}

void *sleep_sort(void *param) {
    if (param == NULL) {
        fprintf(stderr, "sleep_sort: invalid param\n");
        return NULL;
    }
    node_t *node = (node_t *)param;

    while (!all_threads_created){}

    usleep(RATIO * node->s_len);

    if(ERROR == list_insert(node)){
        free(node->string);
        free(node);
    }
    return param;
}

char *read_line(int *is_eof){
    char buf[BUF_SIZE];
    size_t max_buf_size = BUF_SIZE;
    char *string = (char *) calloc(max_buf_size, sizeof(char));
    size_t index = 0;
    while(1){
    	char *res = fgets(buf, BUF_SIZE, stdin);
        if (NO_ERROR != feof(stdin)){
            *is_eof = 1;
            break;
        }
        size_t s_len = strlen(buf);
        if (s_len + index >= max_buf_size - 1){
        	max_buf_size *= 2;
        	string = realloc(string, max_buf_size * sizeof(char));
        	if (string == NULL){
        		perror("read_strings: Unable to reallocate memory for string");
        		break;
        	}
        }
        memcpy(string, buf, BUF_SIZE * sizeof(char));
        index += s_len;
        if (string[s_len - 1] == '\n'){
            index = 0;
            break;
        }
    }        
    return string;
}

int main() {
    int errorCode;
    int num_of_lines = 0;
    int is_eof = 0;
    char *strings[MAX_NUM_OF_LINES];
    pthread_t threads[MAX_NUM_OF_LINES];

    while ((MAX_NUM_OF_LINES > num_of_lines) && !is_eof){
        strings[num_of_lines] = read_line(&is_eof);
        if (NULL == strings[num_of_lines]){
            break;
        }
        num_of_lines++;
    }

    node_t *nodes[MAX_NUM_OF_LINES];
    for (int i = 0; i < num_of_lines; i++){
        nodes[i] = (node_t *) malloc(sizeof (node_t));
        if (NULL == nodes[i]){
            perror("Unable to allocate memory for node");
            free(strings[i]);
            continue;
        }
        nodes[i]->string = strings[i];
        nodes[i]->s_len = strlen(strings[i]);
        nodes[i]->next = NULL;
    }

    printf("Finished strings reading. Sorting started...\n");

    for (int i = 0; i < num_of_lines; i++){
        if (nodes[i] == NULL) {
            continue;
        }

        errorCode = pthread_create(&threads[i], NULL, sleep_sort, nodes[i]);
        if (NO_ERROR != errorCode) {
            print_error("Unable to create thread", errorCode);
            free(strings[i]);
            free(nodes[i]);
            return EXIT_FAILURE;
        }
    }

    all_threads_created = 1;

    for (int i = 0; i < num_of_lines; i++) {
        errorCode = pthread_join(threads[i], NULL);
        if (NO_ERROR != errorCode) {
            print_error("Unable to join thread", errorCode);
            return EXIT_FAILURE;
        }
    }

    if (ERROR == print_list()){
        perror("Unable to print list");
        return EXIT_FAILURE;
    }
    free_list();

    pthread_exit(NULL);
    return EXIT_SUCCESS;
}
