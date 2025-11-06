#include "commands.h"

int _font(int argc, char *argv[]) {
	if (argc < 2) {
		perror("Usage: font <increase|decrease>\n");
		return 1;
	}
	char *arg = argv[1];
	if (strcasecmp(arg, "increase") == 0) {
		return increaseFontSize();
	} else if (strcasecmp(arg, "decrease") == 0) {
		return decreaseFontSize();
	}

	perror("Invalid argument\n");
	return 0;
}