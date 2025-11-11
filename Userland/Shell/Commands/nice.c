
#include "commands.h"

enum {
    NICE_MIN_PRIORITY = 0,
    NICE_MAX_PRIORITY = 2
};

int _nice(int argc, char **argv) {
       if (argc != 3 || argv == NULL || argv[1] == NULL || argv[2] == NULL) {
        fprintf(FD_STDERR, "Usage: nice <pid> <priority>\n");
        return 1;
    }
    const char *command_name = argv[0];
    int pid = 0;
    int priority = 0;

    if (sscanf(argv[1], "%d", &pid) != 1) {
        fprintf(FD_STDERR, "%s: invalid pid '%s'\n", command_name, argv[1]);
        return 1;
    }

    if (sscanf(argv[2], "%d", &priority) != 1) {
        fprintf(FD_STDERR, "%s: invalid priority '%s'\n", command_name, argv[2]);
        return 1;
    }
    if(pid <=2){
        fprintf(FD_STDERR, "%s: the builtin processes cannot be modified\n", command_name);
        return 1;
    }
    if (priority < NICE_MIN_PRIORITY || priority > NICE_MAX_PRIORITY) {
        fprintf(FD_STDERR, "%s: priority must be between %d and %d\n", command_name,
                NICE_MIN_PRIORITY, NICE_MAX_PRIORITY);
        return 1;
    }

    if (nice(pid, priority) != 0) {
        fprintf(FD_STDERR, "%s: unable to change priority of pid %d to %d\n",
                command_name, pid, priority);
        return 1;
    }

    printf("%s: pid %d priority set to %d\n", command_name, pid, priority);
    return 0;
}
