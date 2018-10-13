#include <cairo/cairo-xcb.h>
#include <pango/pango.h>

#include "draw.h"

static void
get_title_height(struct config *cfg, int *height)
{
    if (!cfg->titlemsg) {
        return;
    }
    PangoLayout *pl = pango_cairo_create_layout(cfg->cr);
    pango_layout_set_text(pl, cfg->titlemsg, -1);
    PangoFontDescription *desc =
        pango_font_description_from_string(cfg->titlefont);
    pango_layout_set_font_description(pl, desc);
    pango_font_description_free(desc);
    pango_layout_get_pixel_size(pl, NULL, height);
}

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
    struct color *clr = &cfg->windowcolor;
    cairo_set_source_rgba(cfg->cr, clr->r, clr->g, clr->b, clr->a);
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
        pango_font_description_from_string(cfg->bodyfont);
    pango_layout_set_font_description(cfg->pl, desc);
    pango_font_description_free(desc);

    int title_offset = 0;
    get_title_height(cfg, &title_offset);
    title_offset += title_offset ? cfg->margin * 0.5 : 0;

    struct color *clr = &cfg->bodycolor;
    cairo_set_source_rgba(cfg->cr, clr->r, clr->g, clr->b, clr->a);

    cairo_move_to(cfg->cr, cfg->margin, cfg->margin + title_offset);
    pango_layout_set_width(cfg->pl,
                           (cfg->size[0] - 2 * cfg->margin) * PANGO_SCALE);
    pango_layout_set_alignment(cfg->pl, cfg->bodyalign);
    pango_cairo_update_layout(cfg->cr, cfg->pl);
    pango_cairo_show_layout(cfg->cr, cfg->pl);
}

void
draw_title(struct config *cfg)
{
    if (!cfg->titlemsg) {
        return;
    }
    pango_layout_set_text(cfg->pl, cfg->titlemsg, -1);
    PangoFontDescription *desc =
        pango_font_description_from_string(cfg->titlefont);
    pango_layout_set_font_description(cfg->pl, desc);
    pango_font_description_free(desc);

    struct color *clr = &cfg->titlecolor;
    cairo_set_source_rgba(cfg->cr, clr->r, clr->g, clr->b, clr->a);

    cairo_move_to(cfg->cr, cfg->margin, cfg->margin);
    pango_layout_set_width(cfg->pl,
                           (cfg->size[0] - 2 * cfg->margin) * PANGO_SCALE);
    pango_layout_set_alignment(cfg->pl, cfg->titlealign);
    pango_cairo_update_layout(cfg->cr, cfg->pl);
    pango_cairo_show_layout(cfg->cr, cfg->pl);
}
