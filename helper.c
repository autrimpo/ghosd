#include <stdlib.h>

#include "helper.h"

void
hextorgba(char *hex, SDL_Color *rgba)
{
    char hexpart[3];
    hexpart[2] = '\0';

    hexpart[0] = hex[0];
    hexpart[1] = hex[1];
    rgba->r    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[2];
    hexpart[1] = hex[3];
    rgba->g    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[4];
    hexpart[1] = hex[5];
    rgba->b    = strtol(hexpart, NULL, 16);

    hexpart[0] = hex[6];
    hexpart[1] = hex[7];
    rgba->a    = strtol(hexpart, NULL, 16);
}

void
geomtovec(char *line, int *x, int *y)
{
    int res;
    char *startline, *endline;
    startline = line;
    res       = strtol(startline, &endline, 10);
    *x        = startline == endline ? *x : res;
    startline = endline + 1;
    res       = strtol(startline, &endline, 10);
    *y        = startline == endline ? *y : res;
}

char *
findfont(struct config *cfg, FcChar8 *pattern)
{
    FcPattern *pat = FcNameParse(pattern);
    FcConfigSubstitute(cfg->fccfg, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    FcResult res;
    FcPattern *font = FcFontMatch(cfg->fccfg, pat, &res);
    FcPatternDestroy(pat);
    FcChar8 *fontpath;
    char *ret = NULL;
    if (FcPatternGetString(font, FC_FILE, 0, &fontpath) == FcResultMatch) {
        ret = malloc(strlen((char *)fontpath) + 1);
        strcpy(ret, (char *)fontpath);
    }
    FcPatternDestroy(font);

    return ret;
}
