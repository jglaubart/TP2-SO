
#include "commands.h"

int _time(int argc, char *argv[]) {
	if (argc > 1) {
		perror("Usage: time\n");
		return 1;
	}
    
	int hour, minute, second;
    getDate(&hour, &minute, &second);
    printf("Current time: %xh %xm %xs\n", hour, minute, second);
    return 0;
}