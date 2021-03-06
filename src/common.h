#ifndef GHOSD_COMMON_H
#define GHOSD_COMMON_H

#include <time.h>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#define POSITION_CENTER -1

struct color {
    float r;
    float g;
    float b;
    float a;
};

enum bodytype {
    TEXT,
    BAR,
};

struct config {
    /* xcb */
    xcb_ewmh_connection_t ewmh;
    xcb_connection_t *c;
    xcb_screen_t *screen;
    xcb_drawable_t win;
    xcb_visualtype_t *visual_type;

    /* cairo */
    cairo_t *cr;
    cairo_surface_t *sfc;

    /* pango */
    PangoLayout *pl;

    /* timer */
    timer_t timer;
    struct itimerspec timer_int;

    /* ghosd */
    struct color windowcolor;
    int margin;
    uint32_t pos[2];
    uint32_t size[2];
    struct timespec *timeout;

    PangoAlignment bodyalign;
    struct {
        double height;
        double val;
        double width;
    } bar;
    struct color bodycolor;
    char *defaultbodyfont;
    char *bodyfont;
    char *bodymsg;
    enum bodytype bodytype;

    PangoAlignment titlealign;
    struct color titlecolor;
    char *defaulttitlefont;
    char *titlefont;
    char *titlemsg;
};

#endif
