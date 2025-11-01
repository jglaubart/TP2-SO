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
int shell_kill(void);
int _test_processes(void);


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

    { .name = "malloc",         .function = (int (*)(void))(unsigned long long)mem_test_malloc, .description = "Allocates memory and prints the address.\n\t\t\t\tUse: malloc <size_in_bytes>" },
    { .name = "memstats",       .function = (int (*)(void))(unsigned long long)mem_stats,       .description = "Displays memory statistics (total, used, available)" },
    { .name = "free",           .function = (int (*)(void))(unsigned long long)mem_test_free,   .description = "Frees a previously allocated memory block.\n\t\t\t\tUse: free <address_in_hex>" },
    { .name = "test_mm",       .function = (int (*)(void))(unsigned long long)_test_mm,        .description = "Tests the memory manager by allocating and freeing memory.\n\t\t\t\tUse: _test_mm <max_memory>" },
    { .name = "kill",           .function = (int (*)(void))(unsigned long long)shell_kill,       .description = "Sends a kill signal to the target process.\n\t\t\t\tUse: kill <pid>" },
    { .name = "test_processes", .function = (int (*)(void))(unsigned long long)_test_processes, .description = "Tests process management by creating, blocking and killing processes.\n\t\t\t\tUse: _test_processes <max_processes>" }
};

static void printCommandInfo(const char *name) {
    for (size_t i = 0; i < sizeof(commands) / sizeof(Command); i++) {
        if (strcmp(commands[i].name, name) == 0) {
            printf("* %s%s\t ---\t%s\n", commands[i].name, strlen(commands[i].name) < 4 ? "\t" : "", commands[i].description);
            return;
        }
    }
}


char command_history[HISTORY_SIZE][MAX_BUFFER_SIZE] = {0};
char command_history_buffer[MAX_BUFFER_SIZE] = {0};
uint8_t command_history_last = 0;

static uint64_t last_command_output = 0;

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

        if(buffer_dim == MAX_BUFFER_SIZE){
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
                last_command_output = commands[i].function();
                strncpy(command_history[command_history_last], command_history_buffer, 255);
                command_history[command_history_last][buffer_dim] = '\0';
                INC_MOD(command_history_last, HISTORY_SIZE);
                last_command_arrowed = command_history_last;
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
    static const char *basic_commands[] = {
        "clear", "divzero", "echo", "exit", "font", "help", "history", "invop", "man", "regs", "snake", "time"
    };
    static const char *memory_commands[] = {
        "malloc", "free", "memstats", "test_mm"
    };
    static const char *process_commands[] = {
        "kill", "test_processes"
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
        perror(FD_STDERR, "kill: unable to terminate pid %d\n", pid);
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
        return 1;
    }

    char * args[] = { size_str };
    int32_t pid = createProcess((void *)test_mm, 1, (char **)args);
    return (int)pid;
}

int _test_processes(void){
    char * max_proc_str = strtok(NULL, " ");

    if (max_proc_str == NULL || strtok(NULL, " ") != NULL) {
        perror("Usage: _test_processes <max_processes>\n");
        return 1;
    }

    /* Launch the test as a separate process so the shell isn't disrupted
       if the test blocks/kills processes or runs indefinitely. */
    char * args[] = { max_proc_str };

    int32_t pid = createProcess((void *)test_processes, 1, (char **)args);

    if (pid == -1) {
        perror("_test_processes: failed to create process\n");
        return 1;
    }

    printf("_test_processes: started test as process %d\n", pid);
    return 0;
}
