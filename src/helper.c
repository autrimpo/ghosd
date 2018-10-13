#include <stdlib.h>
#include <string.h>

#include "helper.h"

void
hextorgba(char *hex, struct color *rgba)
{
    char hexpart[3];
    hexpart[2] = '\0';

    hexpart[0] = hex[0];
    hexpart[1] = hex[1];
    rgba->r    = strtol(hexpart, NULL, 16) / 255.0;

    hexpart[0] = hex[2];
    hexpart[1] = hex[3];
    rgba->g    = strtol(hexpart, NULL, 16) / 255.0;

    hexpart[0] = hex[4];
    hexpart[1] = hex[5];
    rgba->b    = strtol(hexpart, NULL, 16) / 255.0;

    hexpart[0] = hex[6];
    hexpart[1] = hex[7];
    rgba->a    = strtol(hexpart, NULL, 16) / 255.0;
}

void
geomtovec(char *line, uint32_t geom[2])
{
    int res;
    char *startline, *endline;
    startline = line;
    res       = strtol(startline, &endline, 10);
    geom[0]   = startline == endline ? geom[0] : res;
    startline = endline + 1;
    res       = strtol(startline, &endline, 10);
    geom[1]   = startline == endline ? geom[1] : res;
}

void
linetostr(char *src, char **dest)
{
    int len = strlen(src);
    *dest   = malloc(len);
    strncpy(*dest, src, len);
    (*dest)[len - 1] = '\0';
}

void
linetoalign(PangoAlignment *align, char *line)
{
    if (!strcmp(line, "left\n")) {
        *align = PANGO_ALIGN_LEFT;
    } else if (!strcmp(line, "right\n")) {
        *align = PANGO_ALIGN_RIGHT;
    } else if (!strcmp(line, "center\n")) {
        *align = PANGO_ALIGN_CENTER;
    }
}
