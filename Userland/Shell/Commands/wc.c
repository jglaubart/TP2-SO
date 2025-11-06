#include "commands.h"

int _wc(int argc, char **argv) {
    if (argc != 1) {
        fprintf(FD_STDERR, "Usage: wc\n");
        return 1;
    }

    static const int buffer_size = 256;
    char buffer[256];
    int32_t read_bytes;
    int lines = 0;

    while ((read_bytes = sys_read(FD_STDIN, (signed char *)buffer, buffer_size)) > 0) {
        for (int i = 0; i < read_bytes; i++) {
            if (buffer[i] == '\n') {
                lines++;
            }
        }
    }

    if (read_bytes < 0) {
        fprintf(FD_STDERR, "wc: read error\n");
        return 1;
    }

    printf("%d\n", lines);
    return 0;
}
