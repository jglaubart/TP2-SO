// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

int _cat(int argc, char **argv) {
    if (argc != 1) {
        fprintf(FD_STDERR, "Usage: cat\n");
        return 1;
    }

    int printed = 0;
    int ch;
    while ((ch = getchar()) != -1) {
        putchar((char)ch);
        printed = 1;
    }

    if (printed) {
        printf("\n\n");
    } else {
        printf("\n\n\n");
    }

    return 0;
}
