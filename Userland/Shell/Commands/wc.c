#include "commands.h"

int _wc(int argc, char **argv) {
    if (argc != 1) {
        fprintf(FD_STDERR, "Usage: wc\n");
        return 1;
    }

    int printed = 0;
    int lines = 0;

    int ch;
    while ((ch = getchar()) != -1) {
        if (ch == '\n') {
            lines++;
        }
        printed = 1;
    }

    if (printed) {
        printf("\n%d\n\n", lines);
    } else {
        printf("\n0\n\n\n");
    }

    return 0;
}
