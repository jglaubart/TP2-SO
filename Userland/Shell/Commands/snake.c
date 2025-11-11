
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
