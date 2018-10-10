#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <fontconfig/fontconfig.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "ghosd.h"
#include "helper.h"

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
setup_sighandler()
{
    struct sigaction action = {0};
    action.sa_handler       = sighandler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGALRM, &action, NULL);
}

void
draw(SDL_Window *win, SDL_Renderer *ren, struct config *cfg)
{
    SDL_SetWindowSize(win, cfg->geom.w, cfg->geom.h);
    SDL_SetWindowPosition(win, cfg->geom.x, cfg->geom.y);
    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, cfg->bg.r, cfg->bg.g, cfg->bg.b, cfg->bg.a);
    SDL_RenderClear(ren);
    if (cfg->bodymsg) {
        SDL_Surface *surf = TTF_RenderText_Blended_Wrapped(
            cfg->font, cfg->bodymsg, cfg->fg, cfg->geom.w - 60);
        SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
        SDL_Rect rect;
        SDL_QueryTexture(tex, NULL, NULL, &rect.w, &rect.h);
        rect.x = (cfg->geom.w - rect.w) / 2;
        rect.y = (cfg->geom.h - rect.h) / 2;

        SDL_RenderCopy(ren, tex, NULL, &rect);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }
    SDL_RenderPresent(ren);
    timer_settime(cfg->timer, 0, &cfg->timer_int, NULL);
}

int
init(SDL_Window **win, SDL_Renderer **ren)
{
    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    *win = SDL_CreateWindow("ghosd", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 640, 460,
                            SDL_WINDOW_HIDDEN | SDL_WINDOW_POPUP_MENU);
    if (!*win) {
        fprintf(stderr, "SDL failed to create a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    *ren = SDL_CreateRenderer(
        *win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*ren) {
        fprintf(stderr, "SDL failed to create a renderer: %s\n",
                SDL_GetError());
        SDL_DestroyWindow(*win);
        SDL_Quit();
        return 1;
    }

    if (TTF_Init()) {
        fprintf(stderr, "SDL_TTF failed to initialize: %s\n", TTF_GetError());
    }

    return 0;
}

int
init_timer(timer_t *timer)
{
    struct sigevent timer_sigev = {0};
    timer_sigev.sigev_notify    = SIGEV_SIGNAL;
    timer_sigev.sigev_signo     = SIGALRM;

    return timer_create(CLOCK_REALTIME, &timer_sigev, timer);
}

void
reset_config(struct config *cfg)
{
    cfg->timer_val->tv_sec  = 1;
    cfg->timer_val->tv_nsec = 0;

    cfg->bg   = (SDL_Color){0, 0, 0, 255};
    cfg->fg   = (SDL_Color){255, 255, 255, 255};
    cfg->geom = (struct geometry){640, 480, SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED};
    cfg->font = cfg->dflt_font;

    check_and_free(cfg->bodymsg);
}

struct config *
init_config()
{
    struct config *cfg = malloc(sizeof(struct config));

    init_timer(&cfg->timer);
    cfg->timer_int.it_interval = (struct timespec){0, 0};
    cfg->timer_val             = &cfg->timer_int.it_value;

    cfg->bodymsg = NULL;

    cfg->fccfg = FcInitLoadConfigAndFonts();

    char *fontpath = findfont(cfg, (FcChar8 *)"monospace");
    cfg->dflt_font = TTF_OpenFont(fontpath, 36);
    free(fontpath);

    reset_config(cfg);
    return cfg;
}

void
destroy_config(struct config *cfg)
{
    check_and_free(cfg->bodymsg);
    FcConfigDestroy(cfg->fccfg);
    timer_delete(cfg->timer);

    if (cfg->font != cfg->dflt_font) {
        TTF_CloseFont(cfg->font);
    }
    TTF_CloseFont(cfg->dflt_font);

    free(cfg);
}

void
destroy(struct config *cfg, SDL_Window *win, SDL_Renderer *ren)
{
    destroy_config(cfg);

    TTF_Quit();

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

int
main(int argc, char **argv)
{
    setup_sighandler();

    FILE *fifo = NULL;
    unlink(GHOSD_FIFO);
    mkfifo(GHOSD_FIFO, 0644);

    SDL_Renderer *ren;
    SDL_Window *win;
    if (init(&win, &ren)) {
        return -1;
    }

    struct config *cfg = init_config();

    size_t ret;
    size_t linelen = 100;
    char *line;

    line = malloc(linelen);
    if (!line) {
        fprintf(stderr, "Failed to allocate a line buffer.\n");
        destroy(cfg, win, ren);
        return -1;
    }

    enum {
        INIT,
        SHOW,
        BG,
        TIMEOUT,
        WINDOWSIZE,
        WINDOWPOS,
        BODYMSG,
    } state;

    run   = 1;
    state = INIT;

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
            ret = getline(&line, &linelen, fifo);
            if (ret == -1 && errno == EINTR) {
                ret = 0;
                continue;
            }
            if (!run || ret == -1) {
                break;
            }
            switch (state) {
            case INIT:
                if (ISCMD("show")) {
                    draw(win, ren, cfg);
                } else if (ISCMD("bg")) {
                    state = BG;
                } else if (ISCMD("timeout")) {
                    state = TIMEOUT;
                } else if (ISCMD("windowsize")) {
                    state = WINDOWSIZE;
                } else if (ISCMD("windowpos")) {
                    state = WINDOWPOS;
                } else if (ISCMD("bodymsg")) {
                    state = BODYMSG;
                }
                break;
            case BG:
                state = INIT;
                hextorgba(line, &cfg->bg);
                break;
            case TIMEOUT:
                state                   = INIT;
                cfg->timer_val->tv_sec  = atoi(line) / 1000;
                cfg->timer_val->tv_nsec = atoi(line) % 1000 * 1000 * 1000;
                break;
            case WINDOWSIZE:
                state = INIT;
                geomtovec(line, &cfg->geom.w, &cfg->geom.h);
                break;
            case WINDOWPOS:
                state = INIT;
                geomtovec(line, &cfg->geom.x, &cfg->geom.y);
                break;
            case BODYMSG:
                state = INIT;
                check_and_free(cfg->bodymsg);
                int len      = strlen(line);
                cfg->bodymsg = malloc(len);
                strncpy(cfg->bodymsg, line, len);
                cfg->bodymsg[len - 1] = '\0';
                break;
            default:
                break;
            }
        }
        fclose(fifo);
        fifo = NULL;
    }

    if (fifo) {
        fclose(fifo);
    }

    check_and_free(line);
    destroy(cfg, win, ren);

    return 0;
}
