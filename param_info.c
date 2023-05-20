#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "param_info.h"

struct param_info_t *pi_ctor(char *dir, char *host_port)
{
    char *delim = ":";
    char *host = strtok(host_port, delim);
    if (!host) {
        fprintf(stderr, "format: '<IP>:<порт>'\n");
        exit(EXIT_FAILURE);
    }

    unsigned port = 0;
    {
        char *pport = strtok(NULL, delim);
        if (!pport) {
            fprintf(stderr, "format: '<IP>:<порт>'\n");
            exit(EXIT_FAILURE);
        }
        long lport = strtol(pport, NULL, 10);
        if (lport < 0 || lport > USHRT_MAX) {
            fprintf(stderr, "incorrect port number 0..%u\n", USHRT_MAX);
            exit(EXIT_FAILURE);
        }
        port = (unsigned)lport;
    }


    struct param_info_t *pi = malloc(sizeof(struct param_info_t));
    pi->dir = strdup(dir);
    pi->host = strdup(host);
    pi->port = port ? port : 5555;

    return pi;
}

void pi_print(struct param_info_t *pi)
{
    printf("--- params ----------\n");
    printf("   dir:  '%s'\n", pi->dir);
    printf("   host: %s\n", pi->host);
    printf("   port: %d\n", pi->port);
    printf("--------------------\n");
}

void pi_dtor(struct param_info_t *pi)
{
    free(pi->dir);
    free(pi->host);
    free(pi);
}


