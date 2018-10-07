#define GHOSD_FIFO "/tmp/ghosd-fifo"

#include <SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct geometry {
    int w;
    int h;
    int x;
    int y;
};

struct config {
    timer_t timer;
    struct itimerspec timer_int;
    SDL_Color bg;
    struct geometry geom;
};

sig_atomic_t run, timed_out;

static void
sighandler(int signo)
{
    switch (signo) {
    case SIGINT:
        run = 0;
        break;
    case SIGALRM:
        timed_out = 1;
        break;
    }
}

void
hextorgba(char *hex, SDL_Color *rgba)
{
    char hexpart[3];
    hexpart[2] = '\0';

    hexpart[0] = hex[0];
    hexpart[1] = hex[1];
    rgba->r    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[2];
    hexpart[1] = hex[3];
    rgba->g    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[4];
    hexpart[1] = hex[5];
    rgba->b    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[6];
    hexpart[1] = hex[7];
    rgba->a    = strtol(hexpart, NULL, 16);
}

void
geomtovec(char *lineptr, int *x, int *y)
{
    int res;
    char *startlineptr, *endlineptr;
    startlineptr = lineptr;
    res          = strtol(startlineptr, &endlineptr, 10);
    *x           = startlineptr == endlineptr ? *x : res;
    startlineptr = endlineptr + 1;
    res          = strtol(startlineptr, &endlineptr, 10);
    *y           = startlineptr == endlineptr ? *y : res;
}

void
draw(SDL_Window *win, SDL_Renderer *ren, struct config *cfg)
{
    SDL_SetWindowSize(win, cfg->geom.w, cfg->geom.h);
    SDL_SetWindowPosition(win, cfg->geom.x, cfg->geom.y);
    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, cfg->bg.r, cfg->bg.g, cfg->bg.b, cfg->bg.a);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);
    timer_settime(cfg->timer, 0, &cfg->timer_int, NULL);
}

int
main(int argc, char **argv)
{
    struct sigaction action = {0};
    action.sa_handler       = sighandler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGALRM, &action, NULL);

    struct sigevent timer_sigev = {0};
    timer_sigev.sigev_notify    = SIGEV_SIGNAL;
    timer_sigev.sigev_signo     = SIGALRM;

    FILE *fifo = NULL;
    unlink(GHOSD_FIFO);
    mkfifo(GHOSD_FIFO, 0644);

    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "ghosd", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 460,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_POPUP_MENU);
    if (!win) {
        fprintf(stderr, "SDL failed to create a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL failed to create a renderer: %s\n",
                SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    struct config config = {
        .timer     = 0,
        .timer_int = {{0}},
        .bg        = {0, 0, 0, 255},
        .geom = {640, 480, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED}};
    timer_create(CLOCK_REALTIME, &timer_sigev, &config.timer);
    config.timer_int.it_value.tv_sec = 1;

    run = 1;

    size_t ret;
    size_t linelen = 100;
    char *lineptr;

    lineptr = malloc(linelen);
    if (!lineptr) {
        fprintf(stderr, "Failed to allocate a line buffer.\n");
    }

    while (run) {
        fifo = fopen(GHOSD_FIFO, "r");
        if (!fifo && errno == EINTR) {
            if (timed_out) {
                SDL_HideWindow(win);
                timed_out = 0;
            }
            continue;
        }
        if (!run) {
            break;
        }
        ret = 0;
        while (ret != -1) {
            ret = getline(&lineptr, &linelen, fifo);
            if (ret == -1 && errno == EINTR) {
                ret = 0;
                continue;
            }
            if (!run || ret == -1) {
                break;
            }
            if (ret == 5 && !strncmp(lineptr, "show\n", 5)) {
                draw(win, ren, &config);
            } else if (ret == 12 && !strncmp(lineptr, "bg=", 3)) {
                hextorgba(lineptr + 3, &config.bg);
            } else if (ret >= 10 && !strncmp(lineptr, "timeout=", 8)) {
                config.timer_int.it_value.tv_sec = atoi(lineptr + 8);
            } else if (ret >= 14 && !strncmp(lineptr, "windowsize=", 11)) {
                geomtovec(lineptr + 11, &config.geom.w, &config.geom.h);
            } else if (ret >= 13 && !strncmp(lineptr, "windowpos=", 10)) {
                geomtovec(lineptr + 10, &config.geom.x, &config.geom.y);
            }
        }
        fclose(fifo);
        fifo = NULL;
    }

    if (fifo) {
        fclose(fifo);
    }

    free(lineptr);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
