#ifndef GHOSD_H
#define GHOSD_H

#include "common.h"

#define GHOSD_FIFO "/tmp/ghosd-fifo"

#define check_and_free(ptr)                                                    \
    if (ptr)                                                                   \
        free(ptr);                                                             \
    ptr = NULL;

#define ISCMD(CMD)                                                             \
    (ret == strlen(CMD "\n") && !strncmp(line, CMD "\n", strlen(CMD "\n")))

void setup_sighandler();
void draw(SDL_Window *win, SDL_Renderer *ren, struct config *cfg);
int init(SDL_Window **win, SDL_Renderer **ren);

int init_timer(timer_t *timer);
void reset_config(struct config *cfg);
struct config *init_config();
void destroy_config(struct config *cfg);
void destroy(struct config *cfg, SDL_Window *win, SDL_Renderer *ren);

#endif
