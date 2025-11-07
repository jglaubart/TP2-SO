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
#define DEFAULT_WRITER_LOOPS 20
#define DEFAULT_READER_LOOPS 20
#define CHILD_LOOP_ARG "30"

#define MVAR_EMPTY_SEM_NAME "mvar-empty"
#define MVAR_FULL_SEM_NAME "mvar-full"

typedef struct {
	void *sem_empty;
	void *sem_full;
	volatile char slot;
	uint8_t initialized;
} KernelMVar;

static KernelMVar kernel_mvar = {0};

static void busy_wait(uint64_t iter);
static uint32_t rng_next(uint32_t seed);
static const char *color_of_reader(const char *idstr);
static int kernel_mvar_open(void);
static int kernel_mvar_put(char value);
static int kernel_mvar_take(char *value);
static int writerMain(int argc, char **argv);
static int readerMain(int argc, char **argv);

static int64_t satoi(char *str) {
  uint64_t i = 0;
  int64_t res = 0;
  int8_t sign = 1;

  if (!str)
    return 0;

  if (str[i] == '-') {
    i++;
    sign = -1;
  }

  for (; str[i] != '\0'; ++i) {
    if (str[i] < '0' || str[i] > '9')
      return 0;
    res = res * 10 + str[i] - '0';
  }

  return res * sign;
}

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
	int loops = DEFAULT_WRITER_LOOPS;
	if (argc >= 2 && argv[1] != NULL) {
		int parsed = satoi(argv[1]);
		if (parsed > 0) {
			loops = parsed;
		}
	}

	uint32_t seed = (uint32_t)((getPid() < 0) ? 0 : getPid());

	for (int i = 0; i < loops; i++) {
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
	int loops = DEFAULT_READER_LOOPS;
	if (argc >= 2 && argv[1] != NULL) {
		int parsed = satoi(argv[1]);
		if (parsed > 0) {
			loops = parsed;
		}
	}

	uint32_t seed = (uint32_t)((getPid() < 0) ? 0 : getPid());

	for (int i = 0; i < loops; i++) {
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

	int nW = satoi(argv[1]);
	int nR = satoi(argv[2]);

	if (nW <= 0 || nR <= 0) {
		printf("mvar: invalid writers/readers\n");
		return -1;
	}

	if (kernel_mvar_open() < 0) {
		printf("mvar: error opening kernel mvar\n");
		return -1;
	}

	const char *reminder = "Remember to run 'mvar-close' after this to clean up the kernel MVar";
	const char *reminder_colors[] = {COLOR_YELLOW, COLOR_MAGENTA, COLOR_BLUE};
	for (unsigned i = 0; i < sizeof(reminder_colors) / sizeof(reminder_colors[0]); i++) {
		printf("%s%s%s\n", reminder_colors[i], reminder, COLOR_RESET);
	}

	for (int i = 0; i < nW; i++) {
		char letter[2];
		letter[0] = 'A' + (i % 26);
		letter[1] = '\0';

		char *wargs[] = {letter, (char *)CHILD_LOOP_ARG, NULL};
		if (createProcess((void *)writerMain, 2, (uint8_t **)wargs, 0) < 0) {
			printf("mvar: failed to create writer process\n");
			return -1;
		}
	}

	for (int i = 0; i < nR; i++) {
		char idbuf[3];
		idbuf[0] = 'R';
		idbuf[1] = '0' + (i % 10);
		idbuf[2] = '\0';

		char *rargs[] = {idbuf, (char *)CHILD_LOOP_ARG, NULL};
		if (createProcess((void *)readerMain, 2, (uint8_t **)rargs, 0) < 0) {
			printf("mvar: failed to create reader process\n");
			return -1;
		}
	}

	return 0;
}
