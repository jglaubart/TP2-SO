#include "commands.h"

int _clear(int argc, char *argv[]) {
    if (argc > 1) {
        perror("Usage: clear\n");
        return 1;
    }
    clearScreen();
    return 0;
}