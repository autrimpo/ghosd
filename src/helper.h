#ifndef GHOSD_HELPER_H
#define GHOSD_HELPER_H

#include <pango/pango.h>

#include "common.h"

void hextorgba(char *hex, struct color *rgba);
void geomtovec(char *line, uint32_t geom[2]);
void linetostr(char *src, char **dest);
void linetoalign(PangoAlignment *align, char *line);

#endif
