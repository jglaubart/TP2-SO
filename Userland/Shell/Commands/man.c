// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"
#include "shell.h"

extern Command commands[];
extern const int commands_size;

int _man(int argc, char *argv[]) {
	char *command;

	if (argc < 2 || (command = argv[1]) == NULL) {
		perror("No argument provided\n");
		return 1;
	}

	if (argc > 2) {
		perror("Too many arguments\n");
		return 1;
	}

	for (int i = 0; i < commands_size; i++) {
		const char *candidate = commands[i].name;
		const char *target = command;
		if (candidate == NULL) {
			continue;
		}

		const char *c_ptr = candidate;
		const char *t_ptr = target;
		while (*c_ptr != '\0' && *t_ptr != '\0') {
			char c_lower = (*c_ptr >= 'A' && *c_ptr <= 'Z') ? (*c_ptr - 'A' + 'a') : *c_ptr;
			char t_lower = (*t_ptr >= 'A' && *t_ptr <= 'Z') ? (*t_ptr - 'A' + 'a') : *t_ptr;
			if (c_lower != t_lower) {
				break;
			}
			c_ptr++;
			t_ptr++;
		}

		if (*c_ptr == '\0' && *t_ptr == '\0') {
			printf("Command: %s\nInformation: %s\n", commands[i].name, commands[i].description);
			return 0;
		}
	}

	perror("Command not found\n");
	return 1;
}
