#include "commands.h"
#include <exceptions.h>

int _exception_divzero(int argc, char **argv) {
	if (argc != 1) {
		fprintf(FD_STDERR, "Usage: divzero\n");
		return 1;
	}
	_divzero();
	return 0;
}

int _exception_invop(int argc, char **argv) {
	if (argc != 1) {
		fprintf(FD_STDERR, "Usage: invop\n");
		return 1;
	}
	_invalidopcode();
	return 0;
}
