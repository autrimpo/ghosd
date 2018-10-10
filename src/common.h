#ifndef GHOSD_COMMON_H
#define GHOSD_COMMON_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include <time.h>

struct geometry {
    int w;
    int h;
    int x;
    int y;
};

struct config {
    timer_t timer;
    struct itimerspec timer_int;
    struct timespec *timer_val;
    SDL_Color bg;
    SDL_Color fg;
    struct geometry geom;
    char *bodymsg;
    FcConfig *fccfg;
    TTF_Font *dflt_font;
    TTF_Font *font;
};

#endif
