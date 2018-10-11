#ifndef GHOSD_HELPER_H
#define GHOSD_HELPER_H

#include "common.h"

void hextorgba(char *hex, struct color *rgba);
void geomtovec(char *line, uint32_t geom[2]);

#endif
