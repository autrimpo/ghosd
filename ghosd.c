#define GHOSD_FIFO "/tmp/ghosd-fifo"

#include <SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
        "ghosd", 100, 100, 640, 460, SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS);
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

    run = 1;

    int ret;
    char buffer[100];

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
        ret = fread(buffer, 1, 100, fifo);
        if (!run) {
            break;
        }
        if (ret == EINTR) {
        } else if (ret) {
            SDL_ShowWindow(win);
            timer_settime(timer, 0, &timer_int, NULL);
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
