// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "commands.h"

#ifdef ANSI_4_BIT_COLOR_SUPPORT
#define COLOR_RESET   "\e[0m"
#define COLOR_WHITE   "\e[1;37m"
#define COLOR_RED     "\e[0;31m"
#define COLOR_GREEN   "\e[0;32m"
#define COLOR_CYAN    "\e[0;36m"
#define COLOR_YELLOW  "\e[0;33m"
#define COLOR_BLUE    "\e[0;34m"
#define COLOR_MAGENTA "\e[0;35m"
#else
#define COLOR_RESET   ""
#define COLOR_WHITE   ""
#define COLOR_RED     ""
#define COLOR_GREEN   ""
#define COLOR_CYAN    ""
#define COLOR_YELLOW  ""
#define COLOR_BLUE    ""
#define COLOR_MAGENTA ""
#endif

static const char *const reader_colors[] = {
	COLOR_RED,
	COLOR_GREEN,
	COLOR_CYAN,
	COLOR_YELLOW,
	COLOR_BLUE,
	COLOR_MAGENTA,
	COLOR_WHITE,
};

#define READER_COLOR_COUNT (sizeof(reader_colors) / sizeof(reader_colors[0]))
#define MAX_TRACKED_MVAR_CHILDREN 64

#define MVAR_EMPTY_SEM_NAME "mvar-empty"
#define MVAR_FULL_SEM_NAME "mvar-full"

typedef struct {
	void *sem_empty;
	void *sem_full;
	volatile char slot;
	uint8_t initialized;
} KernelMVar;

static KernelMVar kernel_mvar = {0};
static int numR;
static int numW;
static int mvar_child_pids[MAX_TRACKED_MVAR_CHILDREN];
static int mvar_child_count = 0;
static uint8_t mvar_tracking_initialized = 0;

static void busy_wait(uint64_t iter);
static uint32_t rng_next(uint32_t seed);
static const char *color_of_reader(const char *idstr);
static int parse_loop_limit(int argc, char **argv);
static void mvar_tracking_init(void);
static void mvar_reset_tracking(void);
static int mvar_register_child(int pid);
static int mvar_pid_is_alive(int pid);
static int mvar_has_active_children(void);
static int mvar_kill_tracked_children(void);
static void mvar_cleanup_all(void);
static int kernel_mvar_open(void);
static void kernel_mvar_destroy(void);
static int kernel_mvar_put(char value);
static int kernel_mvar_take(char *value);
static int writerMain(int argc, char **argv);
static int readerMain(int argc, char **argv);
int _mvar_close(int argc, char **argv);

static void busy_wait(uint64_t iter) {
	for (uint64_t i = 0; i < iter; i++) {
		yield();
	}
}

static uint32_t rng_next(uint32_t seed) {
	int hour = 0;
	int minute = 0;
	int second = 0;
	getDate(&hour, &minute, &second);

	uint32_t time_mix = (uint32_t)(hour * 3600 + minute * 60 + second);
	int32_t pid = getPid();
	uint32_t pid_component = (pid < 0) ? 0u : (uint32_t)pid;
	uint32_t mixed = seed ^ time_mix ^ pid_component;
	return mixed * 1664525u + 1013904223u;
}

static const char *color_of_reader(const char *idstr) {
	if (READER_COLOR_COUNT == 0) {
		return COLOR_RESET;
	}

	if (idstr != NULL && idstr[0] == 'R' && idstr[1] >= '0' && idstr[1] <= '9') {
		return reader_colors[(idstr[1] - '0') % READER_COLOR_COUNT];
	}

	int32_t pid = getPid();
	if (pid < 0) {
		pid = 0;
	}
	return reader_colors[(uint32_t)pid % READER_COLOR_COUNT];
}

static int parse_loop_limit(int argc, char **argv) {
	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		return -1;
	}
	int parsed = (int)satoi(argv[1]);
	return (parsed > 0) ? parsed : -1;
}

static void mvar_tracking_init(void) {
	if (mvar_tracking_initialized) {
		return;
	}
	for (int i = 0; i < MAX_TRACKED_MVAR_CHILDREN; i++) {
		mvar_child_pids[i] = -1;
	}
	mvar_child_count = 0;
	mvar_tracking_initialized = 1;
}

static void mvar_reset_tracking(void) {
	for (int i = 0; i < MAX_TRACKED_MVAR_CHILDREN; i++) {
		mvar_child_pids[i] = -1;
	}
	mvar_child_count = 0;
}

static int mvar_register_child(int pid) {
	if (pid <= 0) {
		return -1;
	}
	mvar_tracking_init();
	if (mvar_child_count >= MAX_TRACKED_MVAR_CHILDREN) {
		return -1;
	}
	mvar_child_pids[mvar_child_count++] = pid;
	return 0;
}

static int mvar_pid_is_alive(int pid) {
	if (pid <= 0) {
		return 0;
	}
	ProcessInformation info;
	if (getProcessInfo(pid, &info) != 0) {
		return 0;
	}
	return info.state != PROCESS_STATE_TERMINATED;
}

static int mvar_has_active_children(void) {
	mvar_tracking_init();
	for (int i = 0; i < mvar_child_count; i++) {
		if (mvar_pid_is_alive(mvar_child_pids[i])) {
			return 1;
		}
	}
	return 0;
}

static int mvar_kill_tracked_children(void) {
	mvar_tracking_init();
	int killed = 0;
	for (int i = 0; i < mvar_child_count; i++) {
		int pid = mvar_child_pids[i];
		if (pid <= 0) {
			continue;
		}
		if (!mvar_pid_is_alive(pid)) {
			continue;
		}
		if (kill(pid) == 0) {
			killed++;
		}
	}
	return killed;
}

static void mvar_cleanup_all(void) {
	mvar_kill_tracked_children();
	kernel_mvar_destroy();
	mvar_reset_tracking();
}

static int kernel_mvar_open(void) {
	if (kernel_mvar.initialized) {
		return 0;
	}

	kernel_mvar.sem_empty = semInit(MVAR_EMPTY_SEM_NAME, 1);
	kernel_mvar.sem_full = semInit(MVAR_FULL_SEM_NAME, 0);

	if (kernel_mvar.sem_empty == NULL || kernel_mvar.sem_full == NULL) {
		if (kernel_mvar.sem_empty != NULL) {
			semDestroy(kernel_mvar.sem_empty);
			kernel_mvar.sem_empty = NULL;
		}
		if (kernel_mvar.sem_full != NULL) {
			semDestroy(kernel_mvar.sem_full);
			kernel_mvar.sem_full = NULL;
		}
		kernel_mvar.initialized = 0;
		return -1;
	}

	kernel_mvar.slot = 0;
	kernel_mvar.initialized = 1;
	return 0;
}

static void kernel_mvar_destroy(void) {
	if (!kernel_mvar.initialized) {
		return;
	}

	if (kernel_mvar.sem_empty != NULL) {
		semDestroy(kernel_mvar.sem_empty);
		kernel_mvar.sem_empty = NULL;
	}
	if (kernel_mvar.sem_full != NULL) {
		semDestroy(kernel_mvar.sem_full);
		kernel_mvar.sem_full = NULL;
	}
	kernel_mvar.slot = 0;
	kernel_mvar.initialized = 0;
}

static int kernel_mvar_put(char value) {
	if (!kernel_mvar.initialized) {
		return -1;
	}

	if (semWait(kernel_mvar.sem_empty) < 0) {
		return -1;
	}

	kernel_mvar.slot = value;

	if (semPost(kernel_mvar.sem_full) < 0) {
		return -1;
	}

	return 0;
}

static int kernel_mvar_take(char *value) {
	if (!kernel_mvar.initialized || value == NULL) {
		return -1;
	}

	if (semWait(kernel_mvar.sem_full) < 0) {
		return -1;
	}

	*value = kernel_mvar.slot;

	if (semPost(kernel_mvar.sem_empty) < 0) {
		return -1;
	}

	return 0;
}

static int writerMain(int argc, char **argv) {
	if (argv == NULL || argc < 1 || argv[0] == NULL) {
		return -1;
	}

	char id = argv[0][0];
	int max_loops = parse_loop_limit(argc, argv);

	uint32_t seed = (uint32_t)((getPid() < 0) ? 0 : getPid());

	for (int i = 0; max_loops < 0 || i < max_loops; i++) {
		seed = rng_next(seed);
		busy_wait((seed % 50U) + 1U);

		if (kernel_mvar_put(id) < 0) {
			printf("mvar: writer failed to put value\n");
			return -1;
		}
	}

	return 0;
}

static int readerMain(int argc, char **argv) {
	if (argv == NULL || argc < 1 || argv[0] == NULL) {
		return -1;
	}

	const char *idstr = argv[0];
	int max_loops = parse_loop_limit(argc, argv);

	uint32_t seed = (uint32_t)((getPid() < 0) ? 0 : getPid());

	for (int i = 0; max_loops < 0 || i < max_loops; i++) {
		seed = rng_next(seed);
		busy_wait((seed % 60U) + 1U);

		char value = 0;
		if (kernel_mvar_take(&value) < 0) {
			printf("mvar: reader failed to take value\n");
			return -1;
		}

		const char *reader_color = color_of_reader(idstr);
		printf("%s%c%s", reader_color, value, COLOR_RESET);
	}

	return 0;
}

int _mvar(int argc, char **argv) {
	if (argc != 3) {
		printf("Usage: mvar <writers> <readers>\n");
		return -1;
	}

	numW = satoi(argv[1]);
	numR = satoi(argv[2]);

	if (numW <= 0 || numR <= 0) {
		printf("mvar: invalid writers/readers\n");
		return -1;
	}

	mvar_tracking_init();
	if (mvar_has_active_children()) {
		printf("mvar: readers/writers already running; finish them with 'mvar-kill' first\n");
		return -1;
	}
	if (kernel_mvar.initialized) {
		printf("mvar: kernel MVar already active; run 'mvar-kill' to reset it\n");
		return -1;
	}
	mvar_reset_tracking();

	if (kernel_mvar_open() < 0) {
		printf("mvar: error opening kernel mvar\n");
		return -1;
	}

	const char *reminder = "Press (Ctrl+K) to kill the processes and clean up the kernel MVar";
	printf("%s%s%s\n", COLOR_BLUE, reminder, COLOR_RESET);

	for (int i = 0; i < numW; i++) {
		char letter[2];
		letter[0] = 'A' + (i % 26);
		letter[1] = '\0';

		char *wargs[] = {letter, NULL};
		int32_t pid = createProcess((void *)writerMain, 1, (uint8_t **)wargs, 0);
		if (pid < 0) {
			printf("mvar: failed to create writer process\n");
			mvar_cleanup_all();
			return -1;
		}
		if (mvar_register_child(pid) < 0) {
			printf("mvar: too many mvar processes, aborting\n");
			kill(pid);
			mvar_cleanup_all();
			return -1;
		}
	}

	for (int i = 0; i < numR; i++) {
		char idbuf[3];
		idbuf[0] = 'R';
		idbuf[1] = '0' + (i % 10);
		idbuf[2] = '\0';

		char *rargs[] = {idbuf, NULL};
		int32_t pid = createProcess((void *)readerMain, 1, (uint8_t **)rargs, 0);
		if (pid < 0) {
			printf("mvar: failed to create reader process\n");
			mvar_cleanup_all();
			return -1;
		}
		if (mvar_register_child(pid) < 0) {
			printf("mvar: too many mvar processes, aborting\n");
			kill(pid);
			mvar_cleanup_all();
			return -1;
		}
	}

	return 0;
}

int _mvar_close(int argc, char **argv) {
	if (argc != 1) {
		printf("Usage: mvar-kill\n");
		return -1;
	}

	mvar_tracking_init();

	int killed = mvar_kill_tracked_children();
	kernel_mvar_destroy();
	mvar_reset_tracking();

	printf("mvar-kill: terminated %d processes and released the kernel MVar\n", killed);
	return 0;
}
