#ifndef PARAM_INFO_H
#define PARAM_INFO_H

struct param_info_t {
    char *dir;
    char *host;
    unsigned port;
};

struct param_info_t *pi_ctor(char *dir, char *host_port);
void pi_print(struct param_info_t *pi);
void pi_dtor(struct param_info_t *pi);

#endif // PARAM_INFO_H
