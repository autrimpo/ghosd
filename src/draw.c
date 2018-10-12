#include <cairo/cairo-xcb.h>

#include "draw.h"

void
draw_pos(struct config *cfg)
{
    uint32_t pos[2];
    pos[0] = cfg->pos[0] == POSITION_CENTER
                 ? (cfg->screen->width_in_pixels - cfg->size[0]) / 2
                 : cfg->pos[0];
    pos[1] = cfg->pos[1] == POSITION_CENTER
                 ? (cfg->screen->height_in_pixels - cfg->size[1]) / 2
                 : cfg->pos[1];
    xcb_configure_window(cfg->c, cfg->win,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, pos);
}

void
draw_size(struct config *cfg)
{
    xcb_configure_window(cfg->c, cfg->win,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         cfg->size);
    cairo_xcb_surface_set_size(cfg->sfc, cfg->size[0], cfg->size[1]);
}

void
draw_clear(struct config *cfg)
{
    cairo_save(cfg->cr);
    cairo_set_operator(cfg->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cfg->cr);
    cairo_restore(cfg->cr);
}

void
draw_bg(struct config *cfg)
{
    cairo_set_source_rgba(cfg->cr, cfg->bg.r, cfg->bg.g, cfg->bg.b, cfg->bg.a);
    cairo_paint(cfg->cr);
}

void
draw_body(struct config *cfg)
{
    if (!cfg->bodymsg) {
        return;
    }
    pango_layout_set_text(cfg->pl, cfg->bodymsg, -1);
    PangoFontDescription *desc =
        pango_font_description_from_string("monospace");
    pango_layout_set_font_description(cfg->pl, desc);
    pango_font_description_free(desc);

    cairo_set_source_rgb(cfg->cr, 1, 1, 1);
    cairo_move_to(cfg->cr, cfg->margin, cfg->margin);
    pango_layout_set_width(cfg->pl,
                           (cfg->size[0] - 2 * cfg->margin) * PANGO_SCALE);
    pango_cairo_update_layout(cfg->cr, cfg->pl);
    pango_cairo_show_layout(cfg->cr, cfg->pl);
}