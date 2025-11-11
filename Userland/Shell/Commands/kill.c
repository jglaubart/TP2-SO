
#include "commands.h"

int _shell_kill(int argc, char * argv[]){
    if (argc != 2) {
        fprintf(FD_STDERR, "Usage: kill <pid>\n");
        return 1;
    }

    const char *command_name = argv[0];
    char * pid_str = argv[1];

    int pid = 0;
    if (sscanf(pid_str, "%d", &pid) != 1) {
        fprintf(FD_STDERR, "%s: invalid pid '%s'\n", command_name, pid_str);
        return 1;
    }

    int32_t result = kill(pid);
    if (result != 0) {
        fprintf(FD_STDERR, "%s: unable to terminate pid %d\n", command_name, pid);
        return 1;
    }

    printf("%s: terminated pid %d\n", command_name, pid);
    return 0;
}
