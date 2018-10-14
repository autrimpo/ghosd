#ifndef GHOSD_H
#define GHOSD_H

#include <string.h>

#include "common.h"

#define check_and_free(ptr)                                                    \
    if (ptr)                                                                   \
        free(ptr);                                                             \
    ptr = NULL;

#define S_GET_S(time) time / 1000
#define S_GET_NS(time) time % 1000 * 1000 * 1000

#define ISCMD(CMD)                                                             \
    (ret == strlen(CMD "\n") && !strncmp(line, CMD "\n", strlen(CMD "\n")))

void setup_sighandler();
void draw(struct config *cfg);
int init(struct config *cfg);

int init_timer(timer_t *timer);
void reset_config(struct config *cfg);
void init_config(struct config *cfg);
void destroy_config(struct config *cfg);
void destroy(struct config *cfg, struct config *draw_cfg);

void print_help(char *bin);
void print_version();

#endif
