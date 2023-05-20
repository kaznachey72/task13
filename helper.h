#ifndef HELPER_H
#define HELPER_H

#include <stdbool.h>

struct data_t {
    int fd;
    unsigned status_code;
    char *status_msg;
    char *path;
    char *msg_error;
    char *server_name;
};

void data_dtor(struct data_t *data);
void data_set_status(struct data_t *data, unsigned code);
void data_set_error(struct data_t *data, const char *template);


char *http_create_response(struct data_t *data, bool is_context_bin, size_t content_size, const void *content_data);
char *html_create_response(const char *context_data);
char *html_create_filelist(const char *dir_path);


int fd_set_nonblock(int fd);
int fd_set_reuse(int fd);

int fd_open_server(const char *host, unsigned port);
int fd_epool_ctladd(int epl_fd, int sock_fd, uint32_t events, struct epoll_event *event);


#endif // FD_HELPER_H
