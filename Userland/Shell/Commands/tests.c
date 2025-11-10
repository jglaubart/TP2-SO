// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"
#include "tests.h"

static int report_failure(const char *command, int64_t status) {
	if (status < 0) {
		fprintf(FD_STDERR, "%s: test failed\n", command);
		return 1;
	}
	return 0;
}

int _test_mm(int argc, char **argv) {
	if (argc != 2) {
		fprintf(FD_STDERR, "Usage: test_mm <max_memory>\n");
		return 1;
	}

	int64_t status = (int64_t)test_mm((uint64_t)argc, argv);
	return report_failure(argv[0], status);
}

int _test_processes(int argc, char **argv) {
	if (argc != 2) {
		fprintf(FD_STDERR, "Usage: test_processes <max_processes>\n");
		return 1;
	}

	int64_t status = test_processes((uint64_t)argc, argv);
	return report_failure(argv[0], status);
}

int _test_prio(int argc, char **argv) {
	if (argc != 2) {
		fprintf(FD_STDERR, "Usage: test_prio <max_value>\n");
		return 1;
	}

	int64_t status = (int64_t)test_prio((uint64_t)argc, argv);
	return report_failure(argv[0], status);
}

int _test_sync(int argc, char **argv) {
	if (argc != 3) {
		fprintf(FD_STDERR, "Usage: test_sync <iterations> <use_semaphore:0|1>\n");
		return 1;
	}

	int64_t status = (int64_t)test_sync((uint64_t)(argc - 1), argv + 1);
	return report_failure(argv[0], status);
}

int _test_wait_children(int argc, char **argv) {
	if (argc > 2) {
		fprintf(FD_STDERR, "Usage: test_wait_children [child_count]\n");
		return 1;
	}

	int64_t status = (int64_t)test_wait_children((uint64_t)argc, argv);
	return report_failure(argv[0], status);
}
