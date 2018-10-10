#ifndef GHOSD_HELPER_H
#define GHOSD_HELPER_H

#include "common.h"

void hextorgba(char *hex, SDL_Color *rgba);
void geomtovec(char *line, int *x, int *y);
char *findfont(struct config *cfg, FcChar8 *pattern);

#endif
