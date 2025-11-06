#include "commands.h"

int _cat(int argc, char **argv) {
    if (argc != 1) {
        fprintf(FD_STDERR, "Usage: cat\n");
        return 1;
    }

    static const int buffer_size = 256;
    char buffer[256];
    int32_t read_bytes;

    while ((read_bytes = sys_read(FD_STDIN, (signed char *)buffer, buffer_size)) > 0) {
        sys_write(FD_STDOUT, buffer, read_bytes);
    }

    return (read_bytes < 0) ? 1 : 0;
}
