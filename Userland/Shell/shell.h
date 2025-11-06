#include <stdint.h>

typedef struct {
    const char * name;
    int (*function)(int argc, char *argv[]);
    const char * description;
    uint8_t is_builtin;
} Command;
