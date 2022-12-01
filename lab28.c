#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#define MAX_REQUEST_LEN 1024
#define PAGE_SIZE 25
#define BUF_SIZE 256

#define NO_ERROR 0
#define ERROR -1

void print_error(const char *prefix, int code) {
    char buffer[256];
    if (NO_ERROR != strerror_r(code, buffer, sizeof(buffer))) {
        strcpy(buffer, "(unable to generate error!)");
    }
    fprintf(stderr, "%s: %s\n", prefix, buffer);
}

int main(int argc, char *argv[]){    
    char request[MAX_REQUEST_LEN];
    char request_template[] = "GET / HTTP/1.1\r\nHost: %s\r\n\r\n";
    int errorCode;
    int sockfd;
    const char * port = "80";
    
    if (2 != argc) {
        fprintf(stderr, "Usage: url\n");
        return EXIT_SUCCESS;
    }

    char *url = argv[1];

    struct addrinfo mysock, *res;
    memset(&mysock, 0, sizeof(struct addrinfo));
    mysock.ai_family = AF_INET;
    mysock.ai_socktype = SOCK_STREAM;

    errorCode = getaddrinfo(url, port, &mysock, &res);
    if (NO_ERROR != errorCode){
        print_error("error getaddrinfo", errorCode);
        return EXIT_FAILURE;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ERROR == sockfd){
        print_error("socket error", sockfd);
        return EXIT_FAILURE;
    }

    errorCode = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (NO_ERROR != errorCode){
        print_error("connection error", errorCode);
        return EXIT_FAILURE;
    }

    sprintf(request, request_template, url);  
    size_t request_len = write(sockfd, request, strlen(request));
    if (MAX_REQUEST_LEN <= request_len) {
        perror("request length large\n");
        return EXIT_FAILURE;
    }

    char buffer[BUF_SIZE];
    int buffer_size = 0;
    char is_sockfd_eof = 0;
    unsigned lines_left = PAGE_SIZE;
    
    fd_set readfds, writefds;
    
    while (!is_sockfd_eof || buffer_size){
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        if (!is_sockfd_eof && !buffer_size){
            FD_SET(sockfd, &readfds);
        }

        if (buffer_size && lines_left){
            FD_SET(STDOUT_FILENO, &writefds);
        }

        if(!lines_left){
            FD_SET(STDIN_FILENO, &readfds);
        }
        
        int nfds = select(sockfd + 1, &readfds, &writefds, NULL, NULL);
        if (ERROR == nfds){
            print_error("error select", nfds);
            break;
        } 
        else {
            if (FD_ISSET(sockfd, &readfds)){
                int bytes_read = read(sockfd, buffer + buffer_size, BUF_SIZE - buffer_size);
                if (ERROR == bytes_read){
                    print_error("error read", bytes_read);
                    close(sockfd);
                    return EXIT_FAILURE;
                } else if (bytes_read == 0){
                    is_sockfd_eof = 1;
                    break;
                } else{
                    buffer_size += bytes_read;
                }
            }

            if(FD_ISSET(STDOUT_FILENO, &writefds) && !is_sockfd_eof){
                int pos;
                for (pos = 0; pos < buffer_size; pos++){
                    if (buffer[pos] == '\n'){
                        lines_left--;
                        if (!lines_left){
                            break;
                        }
                    }
                }

                int bytes_written = write(STDOUT_FILENO, buffer, pos);
                if (ERROR == bytes_written){
                    perror("error to write to STDOUT");
                    close(sockfd);
                    return EXIT_FAILURE;
                }
                
                memmove(buffer, buffer + bytes_written, buffer_size - bytes_written);

                buffer_size -= bytes_written;

                if (!lines_left){
                    printf("\n------------Press enter to scroll down------------\n");
                }
            }

            if (FD_ISSET(STDIN_FILENO, &readfds) && !is_sockfd_eof){
                char key;
                read (STDIN_FILENO, &key, 1);
                if (key == '\n'){
                    lines_left = PAGE_SIZE;
                }
            }
        }
    }
    close(sockfd);

    return EXIT_SUCCESS;
}
