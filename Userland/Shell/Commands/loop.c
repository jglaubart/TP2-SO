// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

int _loop(int argc, char *argv[]) {

	int milliseconds = 1000;

	if (argc != 2) {
		perror("Usage: loop <ms>\n");
		return 1;
	}

	if (argv != NULL && argv[1] != NULL && (sscanf(argv[1], "%d", &milliseconds) != 1 || milliseconds <= 0)) {
		perror("Invalid time argument\n");
		return 1;
	}

	while (1) {
		printf("Hello from PID: %d\n", getPid());
		sleep(milliseconds);
	}

	return 0;
}