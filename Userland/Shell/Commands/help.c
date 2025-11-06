#include "commands.h"
#include "shell.h"

extern Command commands[];
extern const int commands_size;

static int names_match(const char *a, const char *b) {
	if (a == NULL || b == NULL) {
		return 0;
	}
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return 0;
		}
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static void printCommandInfo(const char *name) {
    for (size_t i = 0; i < commands_size; i++) {
        if (names_match(commands[i].name, name)) {
            printf("* %s  ---\t%s\n", commands[i].name, commands[i].description);
            return;
        }
    }
}
int _help(int argc, char * argv[]){
    if(argc > 1){
        perror("Usage: help\n");
        return 1;
    }
    char *basic_commands[] = {
        "clear", "divzero", "echo", "exit", "font", "help", "history", "invop", "man", "regs", "snake", "time"
    };
    char *memory_commands[] = {
        "mem"
    };
	char *process_commands[] = {
		"kill", "ps"
	};
	char *test_commands[] = {
		"test_mm", "test_prio", "test_processes", "test_sync", "test_wait_children"
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
