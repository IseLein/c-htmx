
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_RESPONSE_SIZE 262144
#define MAX_REQUEST_SIZE 65536

typedef struct {
    struct addrinfo *address_info;
    int sockfd;
} SocketInfo;

/*
 * Create a listening socket on the provided port
 */
SocketInfo * get_listen_socket(char *port) {
    struct addrinfo hints;
    struct addrinfo *server_info = (struct addrinfo *)malloc(sizeof(struct addrinfo));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(NULL, port, &hints, &server_info)) != 0) {
        fprintf(stderr, "error occured getting address info %s", gai_strerror(status));
        exit(errno);
    }

    int sockfd;
    if ((sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1) {
        perror("error creating socket");
        exit(errno);
    }
    if ((bind(sockfd, server_info->ai_addr, server_info->ai_addrlen)) != 0) {
        perror("error binding socket");
        exit(errno);
    }
    if ((listen(sockfd, 20)) != 0) {
        perror("error listening");
        exit(errno);
    }

    int *fd = (int *)malloc(sizeof(int));
    SocketInfo *socketInfo = (SocketInfo *)malloc(sizeof(SocketInfo));
    socketInfo->sockfd = sockfd;
    socketInfo->address_info = server_info;
    return socketInfo;
}

int send_response(int fd, char *header, char *content_type, void *body, int content_length) {
    char response[MAX_RESPONSE_SIZE];
    response[0] = '\0';

    strcat(response, header);
    strcat(response, "\r\n");
    strcat(response, content_type);
    strcat(response, "\r\n\n");

    int rv = send(fd, response, strlen(response), 0);
    if (rv < 0) {
        perror("send");
    }
    rv = send(fd, body, content_length, 0);
    if (rv < 0) {
        perror("send");
    }

    return rv;
}

/*
 * assume that the first line of the request is of the form:
 * REQ_TYPE /ROUTE HTTP/X
 */
char **parse_request(char *req) {
    char **rv = (char **)malloc(3 * sizeof(char *));

    char *req_type = strtok(req, " ");
    char *route = strtok(NULL, " ");
    char *http = strtok(NULL, "\n");

    *rv = strndup(req_type, strnlen(req_type, 32));
    *(rv + 1) = strndup(route, strnlen(route, 32));
    *(rv + 2) = strndup(http, strnlen(http, 32));

    return rv;
}

/*
 * DEPRECATED: serve_file seems to do the trick
 */
int serve_html(char *filename, int fd, char *header, char *content_type) {
    FILE *html_fp;
    html_fp = fopen(filename, "r");
    if (html_fp == NULL) {
        perror("fopen");
    }
    char response[MAX_RESPONSE_SIZE];
    char c;
    int i = 0;
    while ((c = fgetc(html_fp)) != EOF) {
        response[i++] = c;
    }
    fclose(html_fp);
    return send_response(fd, header, content_type, response, strlen(response));
}

int serve_file(char *filename, int fd, char *header, char *content_type) {
    // first get size of file
    char *buf, *p;
    struct stat buffer;
    int bytes_read, bytes_remaining, total_bytes = 0;

    if (stat(filename, &buffer) == -1) {
        perror("stat");
    }

    if (!(buffer.st_mode & S_IFREG)) {
        fprintf(stderr, "not a regular file");
    }

    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("fopen");
    }

    bytes_remaining = buffer.st_size;
    p = buf = malloc(bytes_remaining);

    if (buf == NULL) {
        perror("malloc");
    }

    while (bytes_read = fread(p, 1, bytes_remaining, fp), bytes_read != 0 && bytes_remaining > 0) {
        if (bytes_read == -1) {
            free(buf);
            perror("fread");
        }

        bytes_remaining -= bytes_read;
        p += bytes_read;
        total_bytes += bytes_read;
    }

    fclose(fp);

    int rv = send_response(fd, header, content_type, buf, total_bytes);
    return rv;
}

void handle_http_request(int fd) {
    char buf[MAX_REQUEST_SIZE];
    int read_val = recv(fd, buf, MAX_REQUEST_SIZE, 0);
    if (read_val < 0) {
        perror("recv");
    }
    // printf("%s", buf);
    char **req_vals = parse_request(buf);
    // printf("rv: %s\n", *req_vals);
    // printf("rv: %s\n", *(req_vals + 1));
    // printf("rv: %s\n", *(req_vals + 2));

    if ((strcmp(*req_vals, "GET")) == 0) {
        if ((strcmp(*(req_vals + 1), "/")) == 0 || (strcmp(*(req_vals + 1), "/index.html") == 0)) {
            serve_file("index.html", fd, "HTTP/1.1 200 OK", "Content-type: text/html");
        } else if ((strcmp(*(req_vals + 1), "/styles.css")) == 0) {
            serve_file("styles.css", fd, "HTTP/1.1 200 OK", "Content-type: text/css");
        } else if ((strcmp(*(req_vals + 1), "/favicon.ico")) == 0) {
            serve_file("favicon.ico", fd, "HTTP/1.1 200 OK", "Content-type: image/x-icon");
        } else if ((strcmp(*(req_vals + 1), "/iselein.png")) == 0) {
            serve_file("iselein.png", fd, "HTTP/1.1 200 OK", "Content-type: image/png");
        } else if ((strcmp(*(req_vals + 1), "/profile.png")) == 0) {
            serve_file("profile.png", fd, "HTTP/1.1 200 OK", "Content-type: image/png");
        } else if ((strcmp(*(req_vals + 1), "/resume.pdf")) == 0) {
            serve_file("resume.pdf", fd, "HTTP/1.1 200 OK", "Content-type: application/pdf");
        } else {
            serve_file("404.html", fd, "HTTP/1.1 404 NOT FOUND", "Content-type: text/html");
        }
    } else {
        serve_file("404.html", fd, "HTTP/1.1 404 NOT FOUND", "Content-type: text/html");
    }

    free(*req_vals);
    free(*(req_vals + 1));
    free(*(req_vals + 2));
    free(req_vals);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: ./server PORT\n");
        exit(1);
    }
    printf("iselein.me server starting on port %s...\n", *(argv + 1));

    SocketInfo *socketInfo = get_listen_socket(*(argv + 1));
    int sockfd = socketInfo->sockfd;

    printf("waiting for requests...\n");

    int new_sockfd;
    while (1) {
        if ((new_sockfd = accept(sockfd, NULL, NULL)) == -1) {
            perror("accept");
            exit(errno);
        }
        // printf("connected to client\n");
        handle_http_request(new_sockfd);
        // printf("handled request\n");
        close(new_sockfd);
    }
}
