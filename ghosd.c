#define GHOSD_FIFO "/tmp/ghosd-fifo"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    int fifo;
    int readfd;
    unlink(GHOSD_FIFO);
    fifo   = mkfifo(GHOSD_FIFO, 0644);
    readfd = open(GHOSD_FIFO, O_RDONLY);
    char buffer[100];
    while (1) {
        if (read(readfd, buffer, 100)) {
            printf("%s", buffer);
        }
    }
    return 0;
}
