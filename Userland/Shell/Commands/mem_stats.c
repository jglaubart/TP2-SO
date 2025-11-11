

#include "commands.h"

int _mem_stats(int argc, char * argv[]) {
    if (argc > 1) {
		perror("Usage: mem\n");
		return 1;
	}

    int total = 0, used = 0, available = 0;
    
    mem(&total, &used, &available);
    
    printf("\n\e[0;36m=== Memory Statistics ===\e[0m\n");
    printf("Total memory:     %d bytes (%d KB)\n", total, total / 1024);
    printf("Used memory:      %d bytes (%d KB)\n", used, used / 1024);
    printf("Available memory: %d bytes (%d KB)\n", available, available / 1024);
    printf("Usage: %d%%\n\n", total > 0 ? (used * 100) / total : 0);
    
    return 0;
}