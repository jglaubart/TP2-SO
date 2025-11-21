
#include "commands.h"

int _ps(int argc, char * argv[]){ 
    if (argc > 1) {
		perror("Usage: ps\n");
		return 1;
	}

    static ProcessInformation processInfo[64];
    int count = ps(processInfo);
    if (count < 0){
        perror("ps: syscall failed\n");
        return count;
    }
    if (count == 0){
        printf("No processes found\n");
        return 0;
    }

    const char * colors[] = {
        [PROCESS_STATE_READY]      = "\e[0;33m",
        [PROCESS_STATE_RUNNING]    = "\e[0;32m",
        [PROCESS_STATE_BLOCKED]    = "\e[0;31m",
        [PROCESS_STATE_TERMINATED] = "\e[0;90m"
    };
    const char * stateNames[] = {
        [PROCESS_STATE_READY]      = "READY",
        [PROCESS_STATE_RUNNING]    = "RUNNING",
        [PROCESS_STATE_BLOCKED]    = "BLOCKED",
        [PROCESS_STATE_TERMINATED] = "TERMINATED"
    };
    const char * reset = "\e[0m";

    int max_name_length = 4;
	for (int i = 0; i < count; i++) {
        int len = processInfo[i].name[0] ? (int)strlen(processInfo[i].name) : 0;
		if (len > max_name_length) {
			max_name_length = len;
		}
	}

    int header_name_padding = max_name_length - 4;
    if (header_name_padding < 2) {
        header_name_padding = 2;
    }
    char padding_header[header_name_padding + 1];
    int j = 0;
    for (; j < header_name_padding; j++) {
        padding_header[j] = ' ';
    }
    padding_header[j] = '\0';

    const int state_header_target = 10;
    int header_state_padding = state_header_target - 5; /* "State" */
    char padding_state_header[header_state_padding + 1];
    j = 0;
    for (; j < header_state_padding; j++) {
        padding_state_header[j] = ' ';
    }
    padding_state_header[j] = '\0';

	printf("\e[0;0mPID\tName%sState%sPriority\tRSP\t\tRBP\t\tIn FG\n",
	       padding_header, padding_state_header);

    for (int i = 0; i < count; i++) {
        const char *name = processInfo[i].name[0] ? processInfo[i].name : "<unnamed>";
        int name_len = (int)strlen(name);
        int padding_len = max_name_length - name_len + 2;
        if (padding_len < 2) {
            padding_len = 2;
        }
        char padding[padding_len + 1];
        int k = 0;
        for (; k < padding_len; k++) {
            padding[k] = ' ';
        }
        padding[k] = '\0';

        ProcessState state = processInfo[i].state;
        const char *state_color = (state >= PROCESS_STATE_READY && state <= PROCESS_STATE_TERMINATED && colors[state])
                                      ? colors[state]
                                      : reset;
        const char *state_name = (state >= PROCESS_STATE_READY && state <= PROCESS_STATE_TERMINATED && stateNames[state])
                                     ? stateNames[state]
                                     : "UNKNOWN";
        int state_len = (int)strlen(state_name);
        int state_padding_len = state_header_target - state_len;
        if (state_padding_len < 1) {
            state_padding_len = 1;
        }
        char state_padding[state_padding_len + 1];
        k = 0;
        for (; k < state_padding_len; k++) {
            state_padding[k] = ' ';
        }
        state_padding[k] = '\0';

        unsigned int rsp = (unsigned int)(uintptr_t)processInfo[i].rsp;
        unsigned int stack_base = (unsigned int)(uintptr_t)processInfo[i].stack_base;

        printf("%d\t%s%s%s%s%s%s%d\t\t0x%x\t0x%x\t%s\n",
			   processInfo[i].pid, name, padding, state_color, state_name, reset, state_padding,
			   processInfo[i].priority, rsp, stack_base, processInfo[i].is_foreground ? "Yes" : "No");
	}
	return 0;
}
