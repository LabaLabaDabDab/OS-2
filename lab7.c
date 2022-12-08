#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define NO_ERROR 0
#define ERROR -1
#define BUF_SIZE 4096
#define RETRY_SECONDS 5

#define STRINGS_EQUAL(STR1, STR2) (strcmp(STR1, STR2) == 0)
#define IS_STRING_EMPTY(STR) ((STR) == NULL || (STR)[0] == '\0')

typedef struct paths {
    char *src;
    char *dest;
} paths_t;

void *copy_path(void *param);

void print_error(const char *prefix, int code) {
    char buf[256];
    if (NO_ERROR != strerror_r(code, buf, sizeof(buf))) {
        strcpy(buf, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buf);
}

int open_file_with_retry(char *path, int oflag, mode_t mode) {
    errno = 0;
    while (1) {
        int fd = open(path, oflag, mode);
        if (ERROR != fd) {
            return fd;
        }
        if (EMFILE != errno) {
            perror(path);
            break;
        }
        sleep(RETRY_SECONDS);
    }
    return ERROR;
}

DIR *open_dir_with_retry(char *path) {
    errno = 0;
    while (1) {
        DIR *dir = opendir(path);
        if (NULL != dir) {
            return dir;
        }
        if (EMFILE != errno) {
            perror(path);
            break;
        }
        sleep(RETRY_SECONDS);
    }
    return NULL;
}

int create_thread_with_retry(void *param) {
    int errorCode;
    pthread_t thread;
    while (1) {
        errorCode = pthread_create(&thread, NULL, copy_path, param);
        if (NO_ERROR == errorCode) {
            pthread_detach(thread);
            break;
        }
        if (EAGAIN != errorCode) {
            print_error("Unable to create thread", errorCode);
            break;
        }
        sleep(RETRY_SECONDS);
    }
    return errorCode;
}

char *concat_strings(const char **strings) {
    if (NULL == strings) {
        fprintf(stderr, "concat_strings: invalid strings\n");
        return NULL;
    }

    size_t total_length = 0;
    for (int i = 0; strings[i] != NULL; i++) {
        total_length += strlen(strings[i]);
    }

    char *str = calloc(1, (total_length + 1) * sizeof(char));

    if (NULL != str) {
        for (int i = 0; strings[i] != NULL; i++) {
            strcat(str, strings[i]);
        }
    }

    return str;
}

void free_paths(paths_t *paths) {
    if (NULL == paths) {
        return;
    }

    free(paths->src);
    free(paths->dest);
    free(paths);
}

paths_t *build_paths(const char *src_path, const char *dest_path, const char *sub_path) {
    if (NULL == src_path || NULL == dest_path) {
        fprintf(stderr, "build_paths: invalid src_path and/or dest_path\n");
        return NULL;
    }
    
    paths_t *paths = calloc(1, sizeof(paths_t));
    if (NULL == paths) {
        return NULL;
    }

    char *delimiter = IS_STRING_EMPTY(sub_path) ? "" : "/";
    sub_path = IS_STRING_EMPTY(sub_path) ? "" : sub_path;

    const char *full_srt_path[] = { src_path, delimiter, sub_path, NULL };
    const char *full_sub_path[] = { dest_path, delimiter, sub_path, NULL };

    paths->src = concat_strings(full_srt_path);
    paths->dest = concat_strings(full_sub_path);

    if (NULL == paths->src || NULL == paths->dest) {
        free_paths(paths);
        return NULL;
    }

    return paths;
}

void traverse_directory(DIR *dir, struct dirent *entry_buf, paths_t *old_paths) {
    if (NULL == old_paths) {
        fprintf(stderr, "traverse_directory: invalid paths\n");
        return;
    }

    int errorCode;
    struct dirent *result;

    while (1) {
        errorCode = readdir_r(dir, entry_buf, &result);
        if (NO_ERROR != errorCode) {
            print_error(old_paths->src, errorCode);
            break;
        }
        if (NULL == result) {
            break;
        }

        if (STRINGS_EQUAL(entry_buf->d_name, ".") || STRINGS_EQUAL(entry_buf->d_name, "..")) {
            continue;
        }

        paths_t *new_paths = build_paths(old_paths->src, old_paths->dest, entry_buf->d_name);
        if (NULL == new_paths) {
            continue;
        }

        if (NO_ERROR != create_thread_with_retry(new_paths)) {
            free_paths(new_paths);
            continue;
        }
    }
}

void copy_directory(paths_t *paths, mode_t mode) {
    if (NULL == paths || NULL == paths->src || NULL == paths->dest) {
        fprintf(stderr, "copy_directory: invalid paths\n");
        return;
    }

    errno = 0;
    if (ERROR == mkdir(paths->dest, mode) && errno == EEXIST) {
        perror(paths->dest);
        //exit(0);
        return;
    }

    DIR *dir = open_dir_with_retry(paths->src);
    if (NULL == dir) {
        return;
    }
    
    struct dirent *entry = calloc(1 ,sizeof(struct dirent) + pathconf(paths->src, _PC_NAME_MAX) + 1);

    if (NULL == entry) {
        closedir(dir);
        return;
    }

    traverse_directory(dir, entry, paths);

    free(entry);
    if (ERROR == closedir(dir)) {
        perror(paths->src);
    }
}

void copy_file_content(int src_fd, int dest_fd, paths_t *paths) {
    if (NULL == paths) {
        fprintf(stderr, "copy_file_content: invalid paths\n");
        return;
    }

    char buf[BUF_SIZE];
    while (1) {
        ssize_t bytes_read = read(src_fd, buf, BUF_SIZE);
        if (ERROR == bytes_read) {
            perror(paths->src);
            return;
        }
        if (0 == bytes_read) {
            break;
        }

        ssize_t offset = 0;
        ssize_t bytes_written;
        while (offset < bytes_read) {
            bytes_written = write(dest_fd, buf + offset, bytes_read - offset);
            if (ERROR == bytes_written) {
                perror(paths->dest);
                return;
            }
            offset += bytes_written;
        }
    }
}

void copy_regular_file(paths_t *paths, mode_t mode) {
    if (NULL == paths || NULL == paths->src || NULL == paths->dest) {
        fprintf(stderr, "copy_regular_file: invalid paths\n");
        return;
    }

    int src_fd = open_file_with_retry(paths->src, O_RDONLY, mode);
    if (ERROR == src_fd) {
        return;
    }

    int dest_fd = open_file_with_retry(paths->dest, O_WRONLY | O_CREAT | O_EXCL, mode);
    if (ERROR == dest_fd) {
        close(src_fd);
        return;
    }

    copy_file_content(src_fd, dest_fd, paths);

    if (ERROR == close(src_fd)) {
        perror(paths->src);
    }
    if (ERROR == close(dest_fd)) {
        perror(paths->dest);
    }
}

void *copy_path(void *param) {
    if (NULL == param) {
        fprintf(stderr, "copy_path: invalid param\n");
        return NULL;
    }

    paths_t *paths = (paths_t *)param;
    struct stat stat_buf;

    if (ERROR == lstat(paths->src, &stat_buf)) {
        perror(paths->src);
        free_paths(paths);
        return NULL;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
        copy_directory(paths, stat_buf.st_mode);
    }
    else if (S_ISREG(stat_buf.st_mode)) {
        copy_regular_file(paths, stat_buf.st_mode);
    }

    free_paths(paths);
    return NULL;
}

int main(int argc, char **argv) {
    if (3 != argc) {
        fprintf(stderr, "Usage: %s src_path dest_path\n", argv[0]);
        return EXIT_SUCCESS;
    }

    paths_t *paths = build_paths(argv[1], argv[2], "");
    if (NULL == paths) {
        return EXIT_FAILURE;
    }

    copy_path(paths);

    pthread_exit(NULL);
    return EXIT_FAILURE;
}
