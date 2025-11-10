// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

static void *const snakeModuleAddress = (void *)0x500000;

int _snake(int argc, char **argv) {
	(void)argv;
	if (argc != 1) {
		fprintf(FD_STDERR, "Usage: snake\n");
		return 1;
	}
	return exec(snakeModuleAddress);
}
