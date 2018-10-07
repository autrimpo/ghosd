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

struct config {
    SDL_Color bg;
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
    hexpart[3] = '\0';

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
draw(SDL_Window *win, SDL_Renderer *ren, struct config *cfg)
{
    SDL_ShowWindow(win);
    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, cfg->bg.r, cfg->bg.g, cfg->bg.b, cfg->bg.a);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);
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

    timer_t timer;
    timer_create(CLOCK_REALTIME, &timer_sigev, &timer);
    struct itimerspec timer_int = {0};
    timer_int.it_value.tv_sec   = 1;

    FILE *fifo = NULL;
    unlink(GHOSD_FIFO);
    mkfifo(GHOSD_FIFO, 0644);

    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "ghosd", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 460,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS);
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

    struct config config = {.bg = {0, 0, 0, 255}};

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
        ret = getline(&lineptr, &linelen, fifo);
        if (!run) {
            break;
        }
        if (ret != -1) {
            if (!strncmp(lineptr, "show\n", 5)) {
                draw(win, ren, &config);
                timer_settime(timer, 0, &timer_int, NULL);
            } else if (!strncmp(lineptr, "bg=", 3)) {
                hextorgba(lineptr + 3, &config.bg);
            }
        }
        fclose(fifo);
        fifo = NULL;
    }

    if (fifo) {
        fclose(fifo);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
