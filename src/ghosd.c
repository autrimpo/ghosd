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
sync_configs(struct config *main, struct config *draw)
{
    check_and_free(draw->bodymsg);
    if (draw->bodyfont != draw->defaultbodyfont) {
        free(draw->bodyfont);
    }

    check_and_free(draw->titlemsg);
    if (draw->titlefont != draw->defaulttitlefont) {
        free(draw->titlefont);
    }

    memcpy(draw, main, sizeof(struct config));

    if (main->bodymsg) {
        draw->bodymsg = malloc(strlen(main->bodymsg));
        strcpy(draw->bodymsg, main->bodymsg);
    }

    if (main->bodyfont != main->defaultbodyfont) {
        draw->bodyfont = malloc(strlen(main->bodyfont));
        strcpy(draw->bodyfont, main->bodyfont);
    }

    if (main->titlemsg) {
        draw->titlemsg = malloc(strlen(main->titlemsg));
        strcpy(draw->titlemsg, main->titlemsg);
    }

    if (main->titlefont != main->defaulttitlefont) {
        draw->titlefont = malloc(strlen(main->titlefont));
        strcpy(draw->titlefont, main->titlefont);
    }
}

void
draw(struct config *cfg)
{
    draw_pos(cfg);
    draw_size(cfg);
    draw_clear(cfg);
    draw_bg(cfg);
    draw_title(cfg);
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
    cfg->bodycolor = (struct color)DEFAULT_BODY_COLOR;
    linetobodytype(DEFAULT_BODY_TYPE "\n", &cfg->bodytype);
    cfg->bar.val    = DEFAULT_BODY_BAR_VALUE / 100.0;
    cfg->bar.width  = DEFAULT_BODY_BAR_WIDTH / 100.0;
    cfg->bar.height = DEFAULT_BODY_BAR_HEIGHT / 100.0;
    linetoalign(&cfg->bodyalign, DEFAULT_BODY_TEXT_ALIGN "\n");
    if (cfg->bodyfont != cfg->defaultbodyfont) {
        check_and_free(cfg->bodyfont);
        cfg->bodyfont = cfg->defaultbodyfont;
    }
    check_and_free(cfg->bodymsg);

    linetoalign(&cfg->titlealign, DEFAULT_TITLE_ALIGN "\n");
    cfg->titlecolor = (struct color)DEFAULT_TITLE_COLOR;
    if (cfg->titlefont != cfg->defaulttitlefont) {
        check_and_free(cfg->titlefont);
        cfg->titlefont = cfg->defaulttitlefont;
    }
    check_and_free(cfg->titlemsg);

    cfg->windowcolor      = (struct color)DEFAULT_WINDOW_COLOR;
    cfg->margin           = DEFAULT_WINDOW_MARGIN;
    cfg->pos[0]           = DEFAULT_WINDOW_POSITION_X;
    cfg->pos[1]           = DEFAULT_WINDOW_POSITION_Y;
    cfg->size[0]          = DEFAULT_WINDOW_SIZE_X;
    cfg->size[1]          = DEFAULT_WINDOW_SIZE_Y;
    cfg->timeout->tv_sec  = S_GET_S(DEFAULT_WINDOW_TIMEOUT);
    cfg->timeout->tv_nsec = S_GET_NS(DEFAULT_WINDOW_TIMEOUT);
}

void
init_config(struct config *cfg)
{
    init_timer(&cfg->timer);
    cfg->timer_int.it_interval = (struct timespec){0, 0};
    cfg->timeout               = &cfg->timer_int.it_value;

    cfg->bodymsg         = NULL;
    cfg->bodyfont        = NULL;
    cfg->defaultbodyfont = DEFAULT_BODY_TEXT_FONT;

    cfg->titlemsg         = NULL;
    cfg->titlefont        = NULL;
    cfg->defaulttitlefont = DEFAULT_TITLE_FONT;

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
    FILE *fifo      = NULL;

    if (!fifo_path) {
        fifo_path = DEFAULT_FIFO;
    }

    unlink(fifo_path);
    mkfifo(fifo_path, 0644);

    struct config cfg;
    struct config draw_cfg;

    if (init(&cfg)) {
        return -1;
    }

    draw_cfg.defaultbodyfont = NULL;
    draw_cfg.defaulttitlefont = NULL;
    draw_cfg.bodyfont = NULL;
    draw_cfg.titlefont = NULL;
    draw_cfg.bodymsg = NULL;
    draw_cfg.titlemsg = NULL;
    sync_configs(&cfg, &draw_cfg);

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

    pthread_create(&xev_thread, NULL, xev_handle, (void *)&draw_cfg);

    enum {
        INIT,
        BODYTEXTALIGN,
        BODYBARHEIGHT,
        BODYBARVAL,
        BODYBARWIDTH,
        BODYCOLOR,
        BODYTEXTFONT,
        BODYTEXTVAL,
        BODYTYPE,
        SHOW,
        TITLEALIGN,
        TITLECOLOR,
        TITLEFONT,
        TITLEVAL,
        WINDOWCOLOR,
        WINDOWMARGIN,
        WINDOWPOS,
        WINDOWSIZE,
        WINDOWTIMEOUT,
    } state;

    state = INIT;

    run = 1;

    while (run) {
        if (!fifo) {
            fifo = fopen(fifo_path, "r");
        }
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
        while ((ret = getline(&line, &linelen, fifo)) != -1) {
            switch (state) {
            case INIT:
                if (ISCMD("show")) {
                    pthread_mutex_lock(&lock);
                    sync_configs(&cfg, &draw_cfg);
                    xcb_map_window(cfg.c, cfg.win);
                    xcb_flush(cfg.c);
                    draw(&draw_cfg);
                    pthread_mutex_unlock(&lock);
                    timer_settime(cfg.timer, 0, &cfg.timer_int, NULL);
                } else if (ISCMD("reset")) {
                    reset_config(&cfg);
                    break;
                } else if (ISCMD("quit")) {
                    run = 0;
                    break;
                } else if (ISCMD("body-color")) {
                    state = BODYCOLOR;
                } else if (ISCMD("body-type")) {
                    state = BODYTYPE;
                } else if (ISCMD("body-bar-height")) {
                    state = BODYBARHEIGHT;
                } else if (ISCMD("body-bar-value")) {
                    state = BODYBARVAL;
                } else if (ISCMD("body-bar-width")) {
                    state = BODYBARWIDTH;
                } else if (ISCMD("body-text-align")) {
                    state = BODYTEXTALIGN;
                } else if (ISCMD("body-text-font")) {
                    state = BODYTEXTFONT;
                } else if (ISCMD("body-text-value")) {
                    state = BODYTEXTVAL;
                } else if (ISCMD("title-align")) {
                    state = TITLEALIGN;
                } else if (ISCMD("title-color")) {
                    state = TITLECOLOR;
                } else if (ISCMD("title-font")) {
                    state = TITLEFONT;
                } else if (ISCMD("title-value")) {
                    state = TITLEVAL;
                } else if (ISCMD("window-color")) {
                    state = WINDOWCOLOR;
                } else if (ISCMD("window-margin")) {
                    state = WINDOWMARGIN;
                } else if (ISCMD("window-position")) {
                    state = WINDOWPOS;
                } else if (ISCMD("window-size")) {
                    state = WINDOWSIZE;
                } else if (ISCMD("window-timeout")) {
                    state = WINDOWTIMEOUT;
                }
                break;
            case BODYTEXTALIGN:
                state = INIT;
                linetoalign(&cfg.bodyalign, line);
                break;
            case BODYBARHEIGHT:
                state          = INIT;
                cfg.bar.height = atoi(line) / 100.0;
                break;
            case BODYBARVAL:
                state       = INIT;
                cfg.bar.val = atoi(line) / 100.0;
                break;
            case BODYBARWIDTH:
                state         = INIT;
                cfg.bar.width = atoi(line) / 100.0;
                break;
            case BODYCOLOR:
                state = INIT;
                hextorgba(line, &cfg.bodycolor);
                break;
            case BODYTEXTFONT:
                state = INIT;
                if (cfg.bodyfont != cfg.defaultbodyfont) {
                    free(cfg.bodyfont);
                }
                linetostr(line, &cfg.bodyfont);
                break;
            case BODYTEXTVAL:
                state = INIT;
                check_and_free(cfg.bodymsg);
                linetostr(line, &cfg.bodymsg);
                break;
            case BODYTYPE:
                state = INIT;
                linetobodytype(line, &cfg.bodytype);
                break;
            case TITLEALIGN:
                state = INIT;
                linetoalign(&cfg.titlealign, line);
                break;
            case TITLECOLOR:
                state = INIT;
                hextorgba(line, &cfg.titlecolor);
                break;
            case TITLEFONT:
                state = INIT;
                if (cfg.titlefont != cfg.defaulttitlefont) {
                    free(cfg.titlefont);
                }
                linetostr(line, &cfg.titlefont);
                break;
            case TITLEVAL:
                state = INIT;
                check_and_free(cfg.titlemsg);
                linetostr(line, &cfg.titlemsg);
                break;
            case WINDOWCOLOR:
                state = INIT;
                hextorgba(line, &cfg.windowcolor);
                break;
            case WINDOWMARGIN:
                state      = INIT;
                cfg.margin = atoi(line);
                break;
            case WINDOWPOS:
                state = INIT;
                geomtovec(line, cfg.pos);
                break;
            case WINDOWSIZE:
                state = INIT;
                geomtovec(line, cfg.size);
                break;
            case WINDOWTIMEOUT:
                state                = INIT;
                cfg.timeout->tv_sec  = S_GET_S(atoi(line));
                cfg.timeout->tv_nsec = S_GET_NS(atoi(line));
                break;
            default:
                break;
            }
        }
        if (ret == -1) {
            if (errno == EINTR) {
                clearerr(fifo);
            }
            if (feof(fifo)) {
                clearerr(fifo);
                fclose(fifo);
                fifo = NULL;
            }
        }
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
