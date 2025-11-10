// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "commands.h"

int _block(int argc, char * argv[]) {
    if (argc != 2) {
		perror("Usage: block <pid>\n");
		return 1;
	}

	int pid = 0;
	sscanf(argv[1], "%d", &pid);
	if (pid <= 0) {
		perror("Invalid PID\n");
		return 1;
	}

	ProcessInformation info;
	if (getProcessInfo(pid, &info) != 0) {
		fprintf(FD_STDERR, "block: pid %d not found\n", pid);
		return 1;
	}

	int result = 0;
	if (info.state == PROCESS_STATE_BLOCKED) {
		result = unblock(pid);
		if (result == 0) {
			printf("block: pid %d unblocked\n", pid);
		}
	} else if (info.state == PROCESS_STATE_READY || info.state == PROCESS_STATE_RUNNING) {
		result = block(pid);
		if (result == 0) {
			printf("block: pid %d blocked\n", pid);
		}
	} else {
		fprintf(FD_STDERR, "block: pid %d state cannot be toggled\n", pid);
		return 1;
	}

	if (result != 0) {
		fprintf(FD_STDERR, "block: unable to toggle pid %d\n", pid);
		return 1;
	}

	return 0;
}
