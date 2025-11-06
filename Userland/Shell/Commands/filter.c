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

    static const int buffer_size = 256;
    char in[256];
    char out[256];

    int32_t read_bytes;
    while ((read_bytes = sys_read(FD_STDIN, (signed char *)in, buffer_size)) > 0) {
        int out_len = 0;
        for (int i = 0; i < read_bytes; i++) {
            if (!is_vowel(in[i])) {
                out[out_len++] = in[i];
            }
        }
        if (out_len > 0) {
            sys_write(FD_STDOUT, out, out_len);
        }
    }

    return (read_bytes < 0) ? 1 : 0;
}
