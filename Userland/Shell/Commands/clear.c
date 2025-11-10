// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

int _clear(int argc, char *argv[]) {
    if (argc > 1) {
        perror("Usage: clear\n");
        return 1;
    }
    clearScreen();
    return 0;
}