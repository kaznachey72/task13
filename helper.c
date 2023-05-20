#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "helper.h"

#define BACKLOG 128

void data_dtor(struct data_t *data)
{
    free(data->status_msg);
    free(data->msg_error);
    free(data->path);
    free(data->server_name);
    free(data);
}

void data_set_status(struct data_t *data, unsigned code)
{
    data->status_code = code;
    free(data->status_msg);
    switch (code) {
        case 200: data->status_msg = strdup("OK");                    break;
        case 403: data->status_msg = strdup("Forbidden");             break;
        case 404: data->status_msg = strdup("Not Found");             break;
        case 405: data->status_msg = strdup("Method Not Allowed");    break;
        case 500: data->status_msg = strdup("Internal Server Error"); break;
        default:  data->status_msg = strdup("Unknown Error");         break;
    }
}

void data_set_error(struct data_t *data, const char *template)
{
    size_t len = 1 + snprintf(NULL, 0, template, data->path);
    data->msg_error = realloc(data->msg_error, len);
    if (!data->msg_error) {
        fprintf(stderr, "bad alloc\n");
        exit(EXIT_FAILURE);
    }
    snprintf(data->msg_error, len, template, data->path);
}


char *http_create_response(struct data_t *data, bool is_context_bin, size_t content_size, const void *content_data)
{
    char *http_response = NULL;
    static const char *http_template = "HTTP/1.1 %u %s\r\n"
                                       "Server: %s\r\n"
                                       "Content-Length: %lu\r\n"
                                       "Connection: close\r\n"
                                       "Content-Type: %s; charset=UTF-8\r\n\r\n"
                                       "%s";

    const char *content_type = is_context_bin ? "application/octet-stream" : "text/html"; 

    size_t len = 1 + snprintf(NULL, 0, http_template, data->status_code, data->status_msg, data->server_name, content_size, content_type, content_data);
    http_response = malloc(len);
    if (!http_response) {
        fprintf(stderr, "bad alloc\n");
        exit(EXIT_FAILURE);
    }
    snprintf(http_response, len, http_template, data->status_code, data->status_msg, data->server_name, content_size, content_type, content_data);

    return http_response;
}

char *html_create_response(const char *context_data)
{
    static const char *http_template = "<!DOCTYPE html>                   \n"
                                       "<html lang='ru'>                  \n"
                                       "  <head>                          \n"
                                       "    <meta charset='utf-8' />      \n"
                                       "    <title>Файловый серве</title> \n"
                                       "  </head>                         \n"
                                       "  <body>                          \n"
                                       "    %s                            \n"
                                       "  </body>                         \n"
                                       "</html>                           \n";

    size_t html_size = 1 + snprintf(NULL, 0, http_template, context_data);
    char *html = malloc(html_size);
    if (!html) {
        fprintf(stderr, "bad alloc\n");
        exit(EXIT_FAILURE);
    }
    
    snprintf(html, html_size, http_template, context_data);
    return html;
}

char *html_create_filelist(const char *dir_path)
{
    const char *tmpl_html = "<ul>\n%s\n</ul>";
    const char *tmpl_lst = "\t<li> <a href='/%s'> %s </a></li>\n";
    char *html_lst = NULL;

    {
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(dir_path)) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                struct stat st = { 0 };
                bool is_directory = (stat(ent->d_name, &st) == 0) && (st.st_mode & S_IFDIR);
                if (is_directory) continue;

                size_t old_len = html_lst ? strlen(html_lst) : 0;
                size_t add_len = 1 + snprintf(NULL, 0, tmpl_lst, ent->d_name, ent->d_name);
                html_lst = realloc(html_lst, old_len + add_len);
                if (!html_lst) {
                    fprintf(stderr, "bad alloc\n");
                    exit(EXIT_FAILURE);
                }
                snprintf(html_lst + old_len, add_len, tmpl_lst, ent->d_name, ent->d_name);
            }
            closedir(dir);
        }
    }

    char *html;
    size_t len = 1 + snprintf(NULL, 0, tmpl_html, html_lst);
    html = calloc(len, sizeof(char));
    if (!html) {
        fprintf(stderr, "bad alloc\n");
        exit(EXIT_FAILURE);
    }
    snprintf(html, len, tmpl_html, html_lst);
    free(html_lst);
    return html;
}


int fd_set_nonblock(int fd)
{
    int opts = fcntl(fd, F_GETFL);
    if (opts < 0) {
        fprintf(stderr, "fcntl get error\n");
        return -1;
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, opts) < 0) {
        fprintf(stderr, "fcntl set error\n");
        return -1;
    }

  return 0;
}

int fd_set_reuse(int fd)
{
    int f = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &f, sizeof(f));
}

int fd_open_server(const char *host, unsigned port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket error\n");
        return -1;
    }

    fd_set_nonblock(fd);
    fd_set_reuse(fd);

    struct sockaddr_in addr = { 0 };
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port        = htons(port);
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind error\n");
        return -1;
    }

    if (listen(fd, BACKLOG) < 0) {
        fprintf(stderr, "listen error\n");
        return -1;
    }
    return fd;
}

int fd_epool_ctladd(int epl_fd, int sock_fd, uint32_t events, struct epoll_event *event)
{
    struct data_t *data = calloc(1, sizeof(struct data_t));
    data->fd = sock_fd;

    event->data.ptr = data;
    event->events   = events;
    if (epoll_ctl(epl_fd, EPOLL_CTL_ADD, sock_fd, event) < 0) {
        fprintf(stderr, "epoll_ctl error\n");
        data_dtor(data);
        return -1;
    }
    return 0;
}

