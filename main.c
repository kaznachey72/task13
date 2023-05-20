#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include "param_info.h"
#include "helper.h"


#define MAX_EVENTS 128
static struct epoll_event EVENTS[MAX_EVENTS];
bool IS_LOOP_INTERRUPTED = false;


void sig_handler(int sig)
{
    if (sig == SIGINT) {
        IS_LOOP_INTERRUPTED = true;
    }
}


void do_read(struct data_t *data)
{
    char buffer[2048];
    int len = recv(data->fd, buffer, sizeof(buffer), 0);
    if (len < 0) {
        fprintf(stderr, "read error\n");
        return;
    }

    buffer[len] = 0;
    const char *method = strtok(buffer, " ");

    if (strncmp(method, "GET", 3) == 0) {
        data_set_status(data, 200);
        free(data->msg_error);

        const char *fname = strtok(NULL, " ");
        size_t len_fname = 1 + strlen(fname);

        size_t len = strlen(data->path);
        data->path = realloc(data->path, len + len_fname);
        if (!data->path) {
            fprintf(stderr, "bad alloc\n");
            exit(EXIT_FAILURE);
        }
        strncpy(data->path + len, fname, len_fname);
    }
    else {
        data_set_status(data, 405);
        data->msg_error = strdup("Метод не поддерживается (ожидается GET)");
    }
}

void do_write(struct data_t *data)
{
    int fd;
    off_t file_size = -1;
    void *file_map = MAP_FAILED;
    bool is_directory = false;


    if (data->status_code == 200) {
        if (access(data->path, F_OK) < 0) {
            data_set_status(data, 404);
            data_set_error(data, "Файл не найден: '%s'");
        }
        else if (access(data->path, R_OK) < 0) {
            data_set_status(data, 403);
            data_set_error(data, "Ошибка доступа к файлу: '%s'");
        }
        else {
            {
                struct stat st = { 0 };
                is_directory = (stat(data->path, &st) == 0) && (st.st_mode & S_IFDIR);

            }
            if (!is_directory) {
                struct stat st = { 0 };
                fd = open(data->path, O_RDONLY);
                file_size = ((fd > -1) && (fstat(fd, &st) > -1)) ? st.st_size : -1;
                file_map = (file_size > -1) ? mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd, 0) : MAP_FAILED;
                if (file_map == MAP_FAILED) {
                    data_set_status(data, 500);
                    data->msg_error = strdup("Внутрення ошибка сервера");
                }
            }
        }
    }
     
    {
        char *http_response = NULL;

        if (data->status_code == 200) {
            if (is_directory) {
                char *html_flist = html_create_filelist(data->path);
                char *html = html_create_response(html_flist);
                http_response = http_create_response(data, false, 1 + strlen(html), html);
                free(html_flist);
                free(html);
            }
            else {
                http_response = http_create_response(data, true, file_size, file_map);
                //sendfile(data->fd, fd, 0, file_size); 
            }
        }
        else {
            char *html = html_create_response(data->msg_error);
            http_response = http_create_response(data, false, 1 + strlen(html), html);
            free(html);
        }

        if (http_response) {
            int rc = send(data->fd, http_response, strlen(http_response), 0);
            if (rc < 0) {
                fprintf(stderr, "send error\n");
                exit(EXIT_FAILURE);
            }

            free(http_response);
        }
    }

    if (file_map && file_map != MAP_FAILED) {
        munmap(file_map, file_size);
    }
}

void do_process_err(struct data_t *data)
{
    fprintf(stderr, "fd %d error!\n", data->fd);
}

int main (int argc, char *argv[])
{
    if (argc < 3) {
        printf("USAGE: %s <dir> <IP>:<port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);

    struct param_info_t *param_info = pi_ctor(argv[1], argv[2]);
    pi_print(param_info);

    int srv_fd = fd_open_server(param_info->host, param_info->port);
    if (srv_fd < 0) {
        pi_dtor(param_info);
        exit(EXIT_FAILURE);
    }

    struct epoll_event srv_event;
    int epl_fd = epoll_create(MAX_EVENTS);
    uint32_t events = EPOLLIN | EPOLLET;
    int rc = fd_epool_ctladd(epl_fd, srv_fd, events, &srv_event);
    if (rc < 0) {
        pi_dtor(param_info);
        exit(EXIT_FAILURE);
    }


    struct epoll_event cl_event;
    int num_events = 1;

    while (!IS_LOOP_INTERRUPTED) {
        int num_fds = epoll_wait(epl_fd, EVENTS, MAX_EVENTS, -1);

        for (int i=0; i<num_fds; ++i) {
            struct data_t *ev_data = EVENTS[i].data.ptr;
            free(ev_data->path);         ev_data->path = strdup(param_info->dir);
            free(ev_data->server_name);  ev_data->server_name = strdup(param_info->host);

            if (ev_data->fd == srv_fd) {
                int cl_fd = 0;
                while ((cl_fd = accept(srv_fd, NULL, NULL)) > 0) {
                    if (num_events == MAX_EVENTS - 1) {
                        fprintf(stderr, "event array is full\n");
                        close(cl_fd);
                        break;
                    }

                    fd_set_nonblock(cl_fd);
                    uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
                    int rc = fd_epool_ctladd(epl_fd, cl_fd, events, &cl_event);
                    if (rc < 0) {
                        close(cl_fd);
                        break;
                    }

                    ++num_events;
                }
            }
            else {
                {
                    bool is_idata_ready = EVENTS[i].events & EPOLLIN;
                    bool is_odata_ready = EVENTS[i].events & EPOLLOUT;
                    if (!is_idata_ready && is_odata_ready && (ev_data->status_code == 0)) {
                        continue;
                    }
                }

                if (EVENTS[i].events & EPOLLIN) {
                    do_read(ev_data);
                }
                if (EVENTS[i].events & EPOLLOUT) {
                    do_write(ev_data);
                }
                if (EVENTS[i].events & EPOLLRDHUP) {
                    do_process_err(ev_data);
                }

                epoll_ctl(epl_fd, EPOLL_CTL_DEL, ev_data->fd, &cl_event);
                close(ev_data->fd);
                data_dtor(ev_data);

                --num_events;
            }
        }
    }
 
    data_dtor(srv_event.data.ptr);
    pi_dtor(param_info);

    return EXIT_SUCCESS;
}
