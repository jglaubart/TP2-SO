#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <tests.h>

#include <sys.h>
#include <exceptions.h>
#include <syscalls.h>

#ifdef ANSI_4_BIT_COLOR_SUPPORT
    #include <ansiColors.h>
#endif

static void * const snakeModuleAddress = (void*)0x500000;

#define MAX_BUFFER_SIZE 1024
#define HISTORY_SIZE 10

#define INC_MOD(x, m) x = (((x) + 1) % (m))
#define SUB_MOD(a, b, m) ((a) - (b) < 0 ? (m) - (b) + (a) : (a) - (b))
#define DEC_MOD(x, m) ((x) = SUB_MOD(x, 1, m))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static char buffer[MAX_BUFFER_SIZE];
static int buffer_dim = 0;

int clear(void);
int echo(void);
int exit(void);
int fontdec(void);
int font(void);
int help(void);
int history(void);
int man(void);
int snake(void);
int regs(void);
int time(void);
int mem_test_malloc(void);
int mem_test_free(void);
int mem_stats(void);
int _test_mm(void);
int _test_prio(void);
int shell_kill(void);
int _test_processes(void);
int _ps(void);
int _test_sync(void);
int _test_wait_children(void);


static void printPreviousCommand(enum REGISTERABLE_KEYS scancode);
static void printNextCommand(enum REGISTERABLE_KEYS scancode);

static uint8_t last_command_arrowed = 0;

typedef struct {
    char * name;
    int (*function)(void);
    char * description;
} Command;

/* All available commands. Sorted alphabetically by their name */
Command commands[] = {
    { .name = "clear",          .function = (int (*)(void))(unsigned long long)clear,           .description = "Clears the screen" },
    { .name = "divzero",        .function = (int (*)(void))(unsigned long long)_divzero,        .description = "Generates a division by zero exception" },
    { .name = "echo",           .function = (int (*)(void))(unsigned long long)echo ,           .description = "Prints the input string" },
    { .name = "exit",           .function = (int (*)(void))(unsigned long long)exit,            .description = "Command exits w/ the provided exit code or 0" },
    { .name = "font",           .function = (int (*)(void))(unsigned long long)font,            .description = "Increases or decreases the font size.\n\t\t\t\tUse:\n\t\t\t\t\t  + font increase\n\t\t\t\t\t  + font decrease" },
    { .name = "help",           .function = (int (*)(void))(unsigned long long)help,            .description = "Prints the available commands" },
    { .name = "history",        .function = (int (*)(void))(unsigned long long)history,         .description = "Prints the command history" },
    { .name = "invop",          .function = (int (*)(void))(unsigned long long)_invalidopcode,  .description = "Generates an invalid Opcode exception" },
    { .name = "man",            .function = (int (*)(void))(unsigned long long)man,             .description = "Prints the description of the provided command" },
    { .name = "regs",           .function = (int (*)(void))(unsigned long long)regs,            .description = "Prints the register snapshot, if any" },
    { .name = "snake",          .function = (int (*)(void))(unsigned long long)snake,           .description = "Launches the snake game" },
    { .name = "time",           .function = (int (*)(void))(unsigned long long)time,            .description = "Prints the current time" },

    { .name = "malloc",         .function = (int (*)(void))(unsigned long long)mem_test_malloc, .description = "Allocates memory and prints the address\n\t\t\t\tUse: malloc <size_in_bytes>" },
    { .name = "mem",       .function = (int (*)(void))(unsigned long long)mem_stats,       .description = "Displays memory metrics" },
    { .name = "free",           .function = (int (*)(void))(unsigned long long)mem_test_free,   .description = "Frees a previously allocated memory block\n\t\t\t\tUse: free <address_in_hex>" },
    { .name = "test_mm",        .function = (int (*)(void))(unsigned long long)_test_mm,        .description = "Tests the memory manager\n\t\t\t\tUse: _test_mm <max_memory>" },
    { .name = "kill",           .function = (int (*)(void))(unsigned long long)shell_kill,       .description = "Sends a kill signal to the target process.\n\t\t\t\tUse: kill <pid>" },
    { .name = "ps",             .function = (int (*)(void))(unsigned long long)_ps,               .description = "Displays the list of current processes information" },
    { .name = "test_prio",      .function = (int (*)(void))(unsigned long long)_test_prio,      .description = "Tests process priority scheduling\n\t\t\t\tUse: _test_prio <max_value>" },
    { .name = "test_processes", .function = (int (*)(void))(unsigned long long)_test_processes, .description = "Tests process management\n\t\t\t\tUse: _test_processes <max_processes>" },
    { .name = "test_sync",      .function = (int (*)(void))(unsigned long long)_test_sync,       .description = "Tests process synchronization\n\t\t\t\tUse: _test_sync <num_iterations> <use_semaphore: 0|1>" },
    { .name = "test_wait_children", .function = (int (*)(void))(unsigned long long)_test_wait_children, .description = "Spawns children and waits for all of them with waitChildren()\n\t\t\t\tUse: _test_wait_children [child_count]" }
};

static void printCommandInfo(char *name) {
    for (size_t i = 0; i < sizeof(commands) / sizeof(Command); i++) {
        if (strcmp(commands[i].name, name) == 0) {
            printf("* %s  ---\t%s\n", commands[i].name, commands[i].description);
            return;
        }
    }
}


char command_history[HISTORY_SIZE][MAX_BUFFER_SIZE] = {0};
char command_history_buffer[MAX_BUFFER_SIZE] = {0};
uint8_t command_history_last = 0;

static uint64_t last_command_output = 0;
static uint8_t current_command_background = 0;
static int32_t last_spawned_pid = -1;

int main() {
    clear();

    registerKey(KP_UP_KEY, printPreviousCommand);
    registerKey(KP_DOWN_KEY, printNextCommand);

    help();
    
	while (1) {
        printf("\e[0mshell \e[0;32m$\e[0m ");

        signed char c;

        while(buffer_dim < MAX_BUFFER_SIZE && (c = getchar()) != '\n'){
            command_history_buffer[buffer_dim] = c;
            buffer[buffer_dim++] = c;
        }

        buffer[buffer_dim] = 0;
        command_history_buffer[buffer_dim] = 0;

        int entered_length = buffer_dim;
        uint8_t run_in_background = 0;

        int effective_dim = buffer_dim;
        while (effective_dim > 0 && buffer[effective_dim - 1] == ' ') {
            effective_dim--;
            buffer[effective_dim] = 0;
        }

        if (effective_dim > 0 && buffer[effective_dim - 1] == '&') {
            run_in_background = 1;
            effective_dim--;
            buffer[effective_dim] = 0;

            while (effective_dim > 0 && buffer[effective_dim - 1] == ' ') {
                effective_dim--;
                buffer[effective_dim] = 0;
            }
        }

        buffer_dim = effective_dim;

        if(entered_length == MAX_BUFFER_SIZE){
            perror("\e[0;31mShell buffer overflow\e[0m\n");
            buffer[0] = buffer_dim = 0;
            while (c != '\n') c = getchar();
            continue ;
        };

        buffer[buffer_dim] = 0;
        
        char * command = strtok(buffer, " ");
        int i = 0;

        for (; i < sizeof(commands) / sizeof(Command); i++) {
            if (strcmp(commands[i].name, command) == 0) {
                current_command_background = run_in_background;
                last_spawned_pid = -1;
                last_command_output = commands[i].function();
                current_command_background = 0;

                size_t history_length = entered_length < 255 ? entered_length : 255;
                strncpy(command_history[command_history_last], command_history_buffer, 255);
                command_history[command_history_last][history_length] = '\0';
                INC_MOD(command_history_last, HISTORY_SIZE);
                last_command_arrowed = command_history_last;

                if (!run_in_background && last_spawned_pid > 0) {
                    waitPid(last_spawned_pid);
                }
                last_spawned_pid = -1;
                break;
            }
        }

        // If the command is not found, ignore \n
        if ( i == sizeof(commands) / sizeof(Command) ) {
            if (command != NULL && *command != '\0') {
                fprintf(FD_STDERR, "\e[0;33mCommand not found:\e[0m %s. Type 'help' to see available commands.\n", command);
            } else if (command == NULL) {
                printf("\n");
            }
        }
    
        buffer[0] = buffer_dim = 0;
    }

    __builtin_unreachable();
    return 0;
}

static void printPreviousCommand(enum REGISTERABLE_KEYS scancode) {
    clearInputBuffer();
    last_command_arrowed = SUB_MOD(last_command_arrowed, 1, HISTORY_SIZE);
    if (command_history[last_command_arrowed][0] != 0) {
        fprintf(FD_STDIN, command_history[last_command_arrowed]);
    }
}

static void printNextCommand(enum REGISTERABLE_KEYS scancode) {
    clearInputBuffer();
    last_command_arrowed = (last_command_arrowed + 1) % HISTORY_SIZE;
    if (command_history[last_command_arrowed][0] != 0) {
        fprintf(FD_STDIN, command_history[last_command_arrowed]);
    }
}

int history(void) {
    uint8_t last = command_history_last;
    DEC_MOD(last, HISTORY_SIZE);
    uint8_t i = 0;
    while (i < HISTORY_SIZE && command_history[last][0] != 0) {
        printf("%d. %s\n", i, command_history[last]);
        DEC_MOD(last, HISTORY_SIZE);
        i++;
    }
    return 0;
}

int time(void){
	int hour, minute, second;
    getDate(&hour, &minute, &second);
    printf("Current time: %xh %xm %xs\n", hour, minute, second);
    return 0;
}

int echo(void){
    for (int i = strlen("echo") + 1; i < buffer_dim; i++) {
        switch (buffer[i]) {
            case '\\':
                switch (buffer[i + 1]) {
                    case 'n':
                        printf("\n");
                        i++;
                        break;
                    case 'e':
                    #ifdef ANSI_4_BIT_COLOR_SUPPORT
                        i++;
                        parseANSI(buffer, &i); 
                    #else 
                        while (buffer[i] != 'm') i++; // ignores escape code, assumes valid format
                        i++;
                    #endif
                        break;
                    case 'r': 
                        printf("\r");
                        i++;
                        break;
                    case '\\':
                        i++;
                    default:
                        putchar(buffer[i]);
                        break;
                }
                break;
            case '$':
                if (buffer[i + 1] == '?'){
                    printf("%d", last_command_output);
                    i++;
                    break;
                }
            default:
                putchar(buffer[i]);
                break;
        }
    }
    printf("\n");
    return 0;
}

int help(void){
    char *basic_commands[] = {
        "clear", "divzero", "echo", "exit", "font", "help", "history", "invop", "man", "regs", "snake", "time"
    };
    char *memory_commands[] = {
        "malloc", "free", "mem"
    };
    char *process_commands[] = {
        "kill", "ps"
    };
    char *test_commands[] = {
        "test_mm", "test_prio", "test_processes", "test_sync"
    };

    printf("Available commands:\n\n");

    for (size_t i = 0; i < sizeof(basic_commands) / sizeof(char *); i++) {
        printCommandInfo(basic_commands[i]);
    }

    printf("\n========================= Memory =========================\n");
    for (size_t i = 0; i < sizeof(memory_commands) / sizeof(char *); i++) {
        printCommandInfo(memory_commands[i]);
    }

    printf("\n======================== Processes =========================\n");
    for (size_t i = 0; i < sizeof(process_commands) / sizeof(char *); i++) {
        printCommandInfo(process_commands[i]);
    }

    printf("\n========================= Tests =========================\n");
    for (size_t i = 0; i < sizeof(test_commands) / sizeof(char *); i++) {
        printCommandInfo(test_commands[i]);
    }

    printf("\n");
    return 0;
}

int shell_kill(void){
    char * pid_str = strtok(NULL, " ");
    if (pid_str == NULL) {
        fprintf(FD_STDERR, "Usage: kill <pid>\n");
        return 1;
    }

    int pid = 0;
    if (sscanf(pid_str, "%d", &pid) != 1) {
        fprintf(FD_STDERR, "kill: invalid pid '%s'\n", pid_str);
        return 1;
    }

    int32_t result = kill(pid);
    if (result != 0) {
        fprintf(FD_STDERR, "kill: unable to terminate pid %d\n", pid);
        return 1;
    }

    printf("kill: terminated pid %d\n", pid);
    return 0;
}

int clear(void) {
    clearScreen();
    return 0;
}

int exit(void) {
    char * buffer = strtok(NULL, " ");
    int aux = 0;
    sscanf(buffer, "%d", &aux);
    return aux;
}

int font(void) {
    char * arg = strtok(NULL, " ");
    if (strcasecmp(arg, "increase") == 0) {
        return increaseFontSize();
    } else if (strcasecmp(arg, "decrease") == 0) {
        return decreaseFontSize();
    }
    
    perror("Invalid argument\n");
    return 0;
}

int man(void) {
    char * command = strtok(NULL, " ");

    if (command == NULL) {
        perror("No argument provided\n");
        return 1;
    }

    for (int i = 0; i < sizeof(commands) / sizeof(Command); i++) {
        if (strcasecmp(commands[i].name, command) == 0) {
            printf("Command: %s\nInformation: %s\n", commands[i].name, commands[i].description);
            return 0;
        }
    }

    perror("Command not found\n");
    return 1;
}

int regs(void) {
    const static char * register_names[] = {
        "rax", "rbx", "rcx", "rdx", "rbp", "rdi", "rsi", "r8 ", "r9 ", "r10", "r11", "r12", "r13", "r14", "r15", "rsp", "rip", "rflags"
    };

    int64_t registers[18];

    uint8_t aux = getRegisterSnapshot(registers);
    
    if (aux == 0) {
        perror("No register snapshot available\n");
        return 1;
    }

    printf("Latest register snapshot:\n");

    for (int i = 0; i < 18; i++) {
        printf("\e[0;34m%s\e[0m: %x\n", register_names[i], registers[i]);
    }

    return 0;
}

int snake(void) {
    return exec(snakeModuleAddress);
}

int mem_test_malloc(void) {
    char * size_str = strtok(NULL, " ");
    
    if (size_str == NULL) {
        perror("Usage: malloc <size_in_bytes>\n");
        return 1;
    }
    
    int size = 0;
    sscanf(size_str, "%d", &size);
    
    if (size == 0) {
        perror("Invalid size\n");
        return 1;
    }
    
    void * ptr = sys_malloc(size);
    
    if (ptr == NULL) {
        perror("malloc failed: out of memory\n");
        return 1;
    }
    
    printf("Allocated %d bytes at address: 0x%x\n", size, (unsigned int)(uintptr_t)ptr);
    return 0;
}

int mem_test_free(void) {
    char * addr_str = strtok(NULL, " ");
    
    if (addr_str == NULL) {
        perror("Usage: free <address_in_hex>\n");
        return 1;
    }
    
    // Skip "0x" or "0X" prefix if present
    if (addr_str[0] == '0' && (addr_str[1] == 'x' || addr_str[1] == 'X')) {
        addr_str += 2;
    }
    
    // Check string length to prevent overflow (max 16 hex chars for 64-bit)
    int len = 0;
    for (int i = 0; addr_str[i] != '\0'; i++) {
        len++;
    }
    
    if (len > 16) {
        perror("Invalid address: too long\n");
        return 1;
    }
    
    // Manual hex parsing
    uint64_t address = 0;
    for (int i = 0; addr_str[i] != '\0'; i++) {
        char c = addr_str[i];
        
        // Check for overflow before multiplying
        if (address > (0xFFFFFFFFFFFFFFFFULL / 16)) {
            perror("Invalid address: value too large\n");
            return 1;
        }
        
        address *= 16;
        
        if (c >= '0' && c <= '9') {
            address += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            address += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            address += c - 'A' + 10;
        } else {
            perror("Invalid address format\n");
            return 1;
        }
    }
    
    if (address == 0) {
        perror("Cannot free NULL address\n");
        return 1;
    }
    
    int result = sys_free((void *)address);
    if (result) {
        printf("Freed memory at address: 0x%x\n", address);
    } else {
        perror("Failed to free memory: invalid address or not in heap\n");
    }
    return 0;
}


int mem_stats(void) {
    int total = 0, used = 0, available = 0;
    
    sys_memstats(&total, &used, &available);
    
    printf("\n\e[0;36m=== Memory Statistics ===\e[0m\n");
    printf("Total memory:     %d bytes (%d KB)\n", total, total / 1024);
    printf("Used memory:      %d bytes (%d KB)\n", used, used / 1024);
    printf("Available memory: %d bytes (%d KB)\n", available, available / 1024);
    printf("Usage: %d%%\n\n", total > 0 ? (used * 100) / total : 0);
    
    return 0;
}

int _test_mm(void){ //MODIFICAR, hasta ahora se lanza como funcion, al implementar syscall de procesos lanzar como proceso
    char * size_str = strtok(NULL, " ");

    if (size_str == NULL || strtok(NULL, " ") != NULL) {
        perror("Usage: _test_mm <max_memory>\n");
        return -1;
    }

    uint8_t * args[] = { (uint8_t *)"test_mm", (uint8_t *)size_str };
    int32_t pid = createProcess((void *)test_mm, 2, args, current_command_background);
    if (pid == -1) {
        perror("_test_mm: failed to create process\n");
        return -1;
    }
    last_spawned_pid = pid;
    return 0;
}

int _test_processes(void){
    char * max_proc_str = strtok(NULL, " ");

    if (max_proc_str == NULL || strtok(NULL, " ") != NULL) {
        perror("Usage: _test_processes <max_processes>\n");
        return -1;
    }

    /* Launch the test as a separate process so the shell isn't disrupted
       if the test blocks/kills processes or runs indefinitely. */
    uint8_t * args[] = { (uint8_t *)"test_processes", (uint8_t *)max_proc_str };
    int32_t pid = createProcess((void *)test_processes, 2, args, current_command_background);

    if (pid == -1) {
        perror("_test_processes: failed to create process\n");
        return -1;
    }

    printf("_test_processes: started test as process %d\n", pid);
    last_spawned_pid = pid;
    return 0;
}

int _test_prio(void){
    char * max_value_str = strtok(NULL, " ");

    if (max_value_str == NULL || strtok(NULL, " ") != NULL) {
        perror("Usage: _test_prio <max_value>\n");
        return -1;
    }

    /* Launch the test as a separate process so the shell isn't disrupted
       if the test blocks/kills processes or runs indefinitely. */
    uint8_t * args[] = { (uint8_t *)"test_prio", (uint8_t *)max_value_str };
    int32_t pid = createProcess((void *)test_prio, 2, args, current_command_background);

    if (pid == -1) {
        perror("_test_prio: failed to create process\n");
        return -1;
    }

    printf("_test_prio: started test as process %d\n", pid);
    last_spawned_pid = pid;
    return 0;
}

int _ps(void){ //MODOFICAR, ver de hacer mas corto o mover de archivo
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
    if (header_state_padding < 2) {
        header_state_padding = 2;
    }
    char padding_state_header[header_state_padding + 1];
    j = 0;
    for (; j < header_state_padding; j++) {
        padding_state_header[j] = ' ';
    }
    padding_state_header[j] = '\0';

	printf("\e[0;0mPID\tName%sState%sPriority\tRSP\t\tRBP\t\tIn foreground\n",
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

		// printf("%d\t%s%s%s%s%s%s%d\t\t0x%x\t0x%x\t%s\n",
		// 	   processInfo[i].pid, name, padding, state_color, state_name, reset, state_padding,
		// 	   processInfo[i].priority, rsp, stack_base, processInfo[i].hasForeground == 1 ? "Yes" : "No");
        printf("%d\t%s%s%s%s%s%s%d\t\t0x%x\t0x%x\t%s\n",
			   processInfo[i].pid, name, padding, state_color, state_name, reset, state_padding,
			   processInfo[i].priority, rsp, stack_base, processInfo[i].is_foreground ? "Yes" : "No");
	}
	return 0;
}

int _test_sync(void) {
    char *iterations_str = strtok(NULL, " ");
    char *use_sem_str = strtok(NULL, " ");

    if (iterations_str == NULL || use_sem_str == NULL || strtok(NULL, " ") != NULL) {
        perror("Usage: _test_sync <iterations> <use_sem>\n");
        return -1;
    }

    uint8_t *args[] = { (uint8_t *)iterations_str, (uint8_t *)use_sem_str };
    int32_t pid = createProcess((void *)test_sync, 2, args, current_command_background);

    if (pid == -1) {
        perror("_test_sync: failed to create process\n");
        return -1;
    }

    printf("_test_sync: started test as process %d\n", pid);
    last_spawned_pid = pid;
    return 0;
}

int _test_wait_children(void) {
    char *count_str = strtok(NULL, " ");
    if (count_str != NULL && strtok(NULL, " ") != NULL) {
        perror("Usage: _test_wait_children [child_count]\n");
        return -1;
    }

    uint8_t *args[] = { (uint8_t *)"test_wait_children", NULL };
    uint64_t argc = 1;

    if (count_str != NULL) {
        args[1] = (uint8_t *)count_str;
        argc = 2;
    }

    int32_t pid = createProcess((void *)test_wait_children, argc, args, current_command_background);

    if (pid == -1) {
        perror("_test_wait_children: failed to create process\n");
        return -1;
    }

    printf("_test_wait_children: started test as process %d\n", pid);
    last_spawned_pid = pid;
    return 0;
}
