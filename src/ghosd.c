#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#include "config.h"
#include "draw.h"
#include "ghosd.h"
#include "helper.h"

sig_atomic_t run, timed_out;
pthread_mutex_t lock;

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
draw(struct config *cfg)
{
    draw_pos(cfg);
    draw_size(cfg);
    draw_clear(cfg);
    draw_bg(cfg);
    draw_body(cfg);

    xcb_flush(cfg->c);
}

int
init(struct config *cfg)
{
    init_config(cfg);

    cfg->c = xcb_connect(NULL, NULL);

    xcb_intern_atom_cookie_t *ewmh_cookie =
        xcb_ewmh_init_atoms(cfg->c, &cfg->ewmh);
    if (!xcb_ewmh_init_atoms_replies(&cfg->ewmh, ewmh_cookie, NULL)) {
        fprintf(stderr, "Could not initialize X connection.\n");
        return 1;
    }

    cfg->screen = cfg->ewmh.screens[0];

    xcb_visualtype_t *visual_type = NULL;
    xcb_depth_iterator_t depth_iter =
        xcb_screen_allowed_depths_iterator(cfg->screen);

    if (depth_iter.data) {
        for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
            if (depth_iter.data->depth == 32) {
                for (xcb_visualtype_iterator_t visual_iter =
                         xcb_depth_visuals_iterator(depth_iter.data);
                     visual_iter.rem; xcb_visualtype_next(&visual_iter))
                    visual_type = visual_iter.data;
            }
        }
    }

    xcb_colormap_t colormap = xcb_generate_id(cfg->c);
    xcb_create_colormap(cfg->c, XCB_COLORMAP_ALLOC_NONE, colormap,
                        cfg->screen->root, visual_type->visual_id);

    cfg->win = xcb_generate_id(cfg->c);

    uint32_t valwin[4];
    valwin[0] = 0;
    valwin[1] = 0;
    valwin[2] = XCB_EVENT_MASK_EXPOSURE;
    valwin[3] = colormap;
    xcb_create_window(cfg->c, 32, cfg->win, cfg->screen->root,
                      (cfg->screen->width_in_pixels - cfg->size[0]) / 2,
                      (cfg->screen->height_in_pixels - cfg->size[1]) / 2,
                      cfg->size[0], cfg->size[1], 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_type->visual_id,
                      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL |
                          XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      valwin);

    xcb_ewmh_set_wm_window_type(&cfg->ewmh, cfg->win, 1,
                                &cfg->ewmh._NET_WM_WINDOW_TYPE_NOTIFICATION);

    xcb_flush(cfg->c);

    cfg->sfc = cairo_xcb_surface_create(cfg->c, cfg->win, visual_type,
                                        cfg->size[0], cfg->size[1]);
    cairo_xcb_surface_set_size(cfg->sfc, cfg->size[0], cfg->size[1]);
    cfg->cr = cairo_create(cfg->sfc);

    cfg->pl = pango_cairo_create_layout(cfg->cr);

    pthread_mutex_init(&lock, NULL);

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
    cfg->timeout->tv_sec  = S_GET_S(DEFAULT_TIMEOUT);
    cfg->timeout->tv_nsec = S_GET_NS(DEFAULT_TIMEOUT);

    cfg->bg      = (struct color)DEFAULT_BG;
    cfg->size[0] = DEFAULT_SIZE_X;
    cfg->size[1] = DEFAULT_SIZE_Y;
    cfg->pos[0]  = DEFAULT_POS_X;
    cfg->pos[1]  = DEFAULT_POS_Y;
    cfg->margin  = DEFAULT_MARGIN;

    check_and_free(cfg->bodymsg);
    if (cfg->bodyfont != cfg->defaultbodyfont) {
        check_and_free(cfg->bodyfont);
        cfg->bodyfont = cfg->defaultbodyfont;
    }
}

void
init_config(struct config *cfg)
{
    init_timer(&cfg->timer);
    cfg->timer_int.it_interval = (struct timespec){0, 0};
    cfg->timeout               = &cfg->timer_int.it_value;

    cfg->bodymsg         = NULL;
    cfg->bodyfont        = NULL;
    cfg->defaultbodyfont = DEFAULT_BODY_FONT;

    reset_config(cfg);
}

void
destroy_config(struct config *cfg)
{
    check_and_free(cfg->bodymsg);

    if (cfg->bodyfont != cfg->defaultbodyfont) {
        free(cfg->bodyfont);
    }
    timer_delete(cfg->timer);
}

void
destroy(struct config *cfg)
{
    cairo_surface_destroy(cfg->sfc);
    cairo_destroy(cfg->cr);
    xcb_ewmh_connection_wipe(&cfg->ewmh);
    xcb_disconnect(cfg->c);
    destroy_config(cfg);
    pthread_mutex_destroy(&lock);
}

void *
xev_handle(void *ptr)
{
    struct config *cfg = (struct config *)ptr;
    xcb_generic_event_t *xev;
    while (run) {
        xev = xcb_wait_for_event(cfg->c);
        if (!xev)
            return 0;
        switch (xev->response_type) {
        case XCB_EXPOSE: {
            xcb_expose_event_t *eev = (xcb_expose_event_t *)xev;
            if (eev->count == 0) {
                pthread_mutex_lock(&lock);
                draw(cfg);
                pthread_mutex_unlock(&lock);
            }
        } break;
        }
    }
    return NULL;
}

void
print_help(char *bin)
{
    printf("%s [-h|-v]\n \
\t-h: show help\n \
\t-v: show version\n",
           bin);
}

void
print_version()
{
    printf("ghosd %s\n", VERSION);
}

int
main(int argc, char **argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'v':
            print_version();
            return 0;
        case 'h':
            print_help(argv[0]);
            return 0;
        default:
            print_help(argv[0]);
            return -1;
        }
    }

    setup_sighandler();

    char *fifo_path = getenv("GHOSD_FIFO");
    FILE *fifo;

    if (!fifo_path) {
        fifo_path = DEFAULT_FIFO;
    }

    unlink(fifo_path);
    mkfifo(fifo_path, 0644);

    struct config cfg;

    if (init(&cfg)) {
        return -1;
    }

    size_t ret;
    size_t linelen = 100;
    char *line;

    line = malloc(linelen);
    if (!line) {
        fprintf(stderr, "Failed to allocate a line buffer.\n");
        destroy(&cfg);
        return -1;
    }

    pthread_t xev_thread;

    pthread_create(&xev_thread, NULL, xev_handle, (void *)&cfg);

    enum {
        INIT,
        SHOW,
        BG,
        TIMEOUT,
        WINDOWSIZE,
        WINDOWPOS,
        MARGIN,
        BODYMSG,
        BODYFONT,
    } state;

    state = INIT;

    run = 1;
    int len;

    while (run) {
        fifo = fopen(fifo_path, "r");
        if (!fifo && errno == EINTR) {
            if (timed_out) {
                xcb_unmap_window(cfg.c, cfg.win);
                xcb_flush(cfg.c);
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
                ret   = 0;
                errno = 0;
                continue;
            }
            if (!run || ret == -1) {
                break;
            }
            pthread_mutex_lock(&lock);
            switch (state) {
            case INIT:
                if (ISCMD("show")) {
                    xcb_map_window(cfg.c, cfg.win);
                    xcb_flush(cfg.c);
                    draw(&cfg);
                    timer_settime(cfg.timer, 0, &cfg.timer_int, NULL);
                } else if (ISCMD("window-bg")) {
                    state = BG;
                } else if (ISCMD("window-timeout")) {
                    state = TIMEOUT;
                } else if (ISCMD("window-size")) {
                    state = WINDOWSIZE;
                } else if (ISCMD("window-pos")) {
                    state = WINDOWPOS;
                } else if (ISCMD("window-margin")) {
                    state = MARGIN;
                } else if (ISCMD("body-msg")) {
                    state = BODYMSG;
                } else if (ISCMD("body-font")) {
                    state = BODYFONT;
                } else if (ISCMD("reset")) {
                    reset_config(&cfg);
                    break;
                } else if (ISCMD("quit")) {
                    run = 0;
                    break;
                }
                break;
            case BG:
                state = INIT;
                hextorgba(line, &cfg.bg);
                break;
            case TIMEOUT:
                state                = INIT;
                cfg.timeout->tv_sec  = S_GET_S(atoi(line));
                cfg.timeout->tv_nsec = S_GET_NS(atoi(line));
                break;
            case WINDOWSIZE:
                state = INIT;
                geomtovec(line, cfg.size);
                break;
            case WINDOWPOS:
                state = INIT;
                geomtovec(line, cfg.pos);
                break;
            case BODYMSG:
                state = INIT;
                check_and_free(cfg.bodymsg);
                len         = strlen(line);
                cfg.bodymsg = malloc(len);
                strncpy(cfg.bodymsg, line, len);
                cfg.bodymsg[len - 1] = '\0';
                break;
            case BODYFONT:
                state = INIT;
                if (cfg.bodyfont != cfg.defaultbodyfont) {
                    free(cfg.bodyfont);
                }
                len          = strlen(line);
                cfg.bodyfont = malloc(len);
                strncpy(cfg.bodyfont, line, len);
                cfg.bodyfont[len - 1] = '\0';
                break;
            case MARGIN:
                state      = INIT;
                cfg.margin = atoi(line);
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&lock);
        }
        fclose(fifo);
        fifo = NULL;
    }

    if (fifo) {
        fclose(fifo);
    }

    unlink(fifo_path);

    xcb_expose_event_t event = {0};
    xcb_send_event(cfg.c, 0, cfg.win, XCB_EVENT_MASK_EXPOSURE, (char *)&event);
    xcb_flush(cfg.c);
    pthread_join(xev_thread, NULL);

    check_and_free(line);
    destroy(&cfg);

    return 0;
}
