// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "commands.h"

static int is_vowel(char c) {
    switch (c) {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            return 1;
        default:
            return 0;
    }
}

int _filter(int argc, char **argv) {
    if (argc != 1) {
        fprintf(FD_STDERR, "Usage: filter\n");
        return 1;
    }

    int printed = 0;
    int ch;
    while ((ch = getchar()) != -1) {
        if (!is_vowel((char)ch)) {
            putchar((char)ch);
            printed = 1;
        }
    }

    if (printed) {
        printf("\n\n");
    } else {
        printf("\n\n\n");
    }

    return 0;
}
