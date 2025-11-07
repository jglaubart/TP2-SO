#include <commands.h>

#define BUSY_WAIT_BASE 80000U
#define BUSY_WAIT_JITTER 160000U
#define BUSY_WAIT_YIELD_STEP 4096U
#define NUMBER_BUFFER_LEN 16
#define POINTER_BUFFER_LEN 32
#define SEM_NAME_BUFFER_LEN 64

typedef struct {
	void *sem_empty;
	void *sem_full;
	volatile char slot_value;
	int writer_count;
	int reader_count;
	uint32_t *writer_seeds;
	uint32_t *reader_seeds;
	int32_t *writer_pids;
	int32_t *reader_pids;
} MVarContext;

static const char *const reader_colors[] = {
	"\e[0;31m", "\e[0;32m", "\e[0;33m", "\e[0;34m", "\e[0;35m", "\e[0;36m",
	"\e[1;31m", "\e[1;32m", "\e[1;33m", "\e[1;34m", "\e[1;35m", "\e[1;36m",
};

#define READER_COLOR_COUNT (sizeof(reader_colors) / sizeof(reader_colors[0]))

static int writer_task(int argc, char **argv);
static int reader_task(int argc, char **argv);
static int supervisor_task(int argc, char **argv);
static MVarContext *create_context(int writers, int readers);
static void free_context_memory(MVarContext *ctx);
static uint32_t base_seed(void);
static uint32_t mix_seed(uint32_t base, int salt);
static uint32_t next_random(uint32_t *state);
static void random_busy_wait(uint32_t *state);
static void pointer_to_string(void *ptr, char *buffer, int capacity);
static MVarContext *string_to_context(const char *value);
static void build_number_string(int value, char *buffer, int capacity);
static void build_process_name(const char *prefix, int index, char *buffer, int capacity);
static void build_sem_name(const char *prefix, const char *suffix, char *buffer, int capacity);
static char writer_symbol_for(int index);
static const char *reader_color_for(int index);
static int spawn_writers(MVarContext *ctx, const char *ctx_str);
static int spawn_readers(MVarContext *ctx, const char *ctx_str);
static void kill_spawned_group(int32_t *pids, int count);
static int launch_supervisor(MVarContext *ctx, const char *ctx_str);

int mvar(int argc, char **argv) {
	if (argc != 3) {
		fprintf(FD_STDERR, "Usage: mvar <writers> <readers>\n");
		return 1;
	}

	int writer_count = 0;
	int reader_count = 0;

	if (sscanf(argv[1], "%d", &writer_count) != 1 || writer_count <= 0 ||
		sscanf(argv[2], "%d", &reader_count) != 1 || reader_count <= 0) {
		fprintf(FD_STDERR, "mvar: both writer and reader counts must be positive integers\n");
		return 1;
	}

	MVarContext *ctx = create_context(writer_count, reader_count);
	if (ctx == NULL) {
		fprintf(FD_STDERR, "mvar: unable to allocate context\n");
		return 1;
	}

	char ctx_ref[POINTER_BUFFER_LEN];
	pointer_to_string(ctx, ctx_ref, sizeof(ctx_ref));

	char empty_name[SEM_NAME_BUFFER_LEN];
	char full_name[SEM_NAME_BUFFER_LEN];
	build_sem_name("mvar-empty-", ctx_ref, empty_name, sizeof(empty_name));
	build_sem_name("mvar-full-", ctx_ref, full_name, sizeof(full_name));

	ctx->sem_empty = semInit(empty_name, 1);
	ctx->sem_full = semInit(full_name, 0);
	if (ctx->sem_empty == NULL || ctx->sem_full == NULL) {
		fprintf(FD_STDERR, "mvar: failed to initialize semaphores\n");
		if (ctx->sem_empty != NULL) {
			semDestroy(ctx->sem_empty);
		}
		if (ctx->sem_full != NULL) {
			semDestroy(ctx->sem_full);
		}
		free_context_memory(ctx);
		return 1;
	}

	uint32_t seed = base_seed();
	for (int i = 0; i < writer_count; i++) {
		ctx->writer_seeds[i] = mix_seed(seed, i + 1);
		if (ctx->writer_seeds[i] == 0) {
			ctx->writer_seeds[i] = (uint32_t)(i + 1);
		}
	}
	for (int i = 0; i < reader_count; i++) {
		ctx->reader_seeds[i] = mix_seed(seed ^ 0x9E3779B9u, i + 1);
		if (ctx->reader_seeds[i] == 0) {
			ctx->reader_seeds[i] = (uint32_t)(i + 1);
		}
	}

	if (spawn_writers(ctx, ctx_ref) != 0) {
		fprintf(FD_STDERR, "mvar: failed while creating writers\n");
		kill_spawned_group(ctx->writer_pids, writer_count);
		semDestroy(ctx->sem_empty);
		semDestroy(ctx->sem_full);
		free_context_memory(ctx);
		return 1;
	}

	if (spawn_readers(ctx, ctx_ref) != 0) {
		fprintf(FD_STDERR, "mvar: failed while creating readers\n");
		kill_spawned_group(ctx->writer_pids, writer_count);
		kill_spawned_group(ctx->reader_pids, reader_count);
		semDestroy(ctx->sem_empty);
		semDestroy(ctx->sem_full);
		free_context_memory(ctx);
		return 1;
	}

	if (launch_supervisor(ctx, ctx_ref) != 0) {
		fprintf(FD_STDERR, "mvar: failed to launch supervisor\n");
		kill_spawned_group(ctx->writer_pids, writer_count);
		kill_spawned_group(ctx->reader_pids, reader_count);
		semDestroy(ctx->sem_empty);
		semDestroy(ctx->sem_full);
		free_context_memory(ctx);
		return 1;
	}

	printf("mvar: started %d writer(s) and %d reader(s). Use ps/kill/nice to manage them.\n",
	       writer_count, reader_count);

	return 0;
}

static int spawn_writers(MVarContext *ctx, const char *ctx_str) {
	for (int i = 0; i < ctx->writer_count; i++) {
		char name[PROCESS_NAME_MAX_LENGTH];
		char index_buffer[NUMBER_BUFFER_LEN];
		char *writer_argv[] = {name, index_buffer, (char *)ctx_str, NULL};

		build_process_name("mvar-writer-", i, name, sizeof(name));
		build_number_string(i, index_buffer, sizeof(index_buffer));

		int32_t pid = createProcess((void *)writer_task, 3, (uint8_t **)writer_argv, 0);
		if (pid < 0) {
			return -1;
		}
		ctx->writer_pids[i] = pid;
	}
	return 0;
}

static int spawn_readers(MVarContext *ctx, const char *ctx_str) {
	for (int i = 0; i < ctx->reader_count; i++) {
		char name[PROCESS_NAME_MAX_LENGTH];
		char index_buffer[NUMBER_BUFFER_LEN];
		char *reader_argv[] = {name, index_buffer, (char *)ctx_str, NULL};

		build_process_name("mvar-reader-", i, name, sizeof(name));
		build_number_string(i, index_buffer, sizeof(index_buffer));

		int32_t pid = createProcess((void *)reader_task, 3, (uint8_t **)reader_argv, 0);
		if (pid < 0) {
			return -1;
		}
		ctx->reader_pids[i] = pid;
	}
	return 0;
}

static int launch_supervisor(MVarContext *ctx, const char *ctx_str) {
	char *manager_argv[] = {"mvar-manager", (char *)ctx_str, NULL};
	int32_t pid = createProcess((void *)supervisor_task, 2, (uint8_t **)manager_argv, 1);
	if (pid < 0) {
		return -1;
	}
	return 0;
}

static uint32_t base_seed(void) {
	int hour = 0, minute = 0, second = 0;
	getDate(&hour, &minute, &second);
	uint32_t pid = (uint32_t)getPid();
	return (uint32_t)(second + minute * 60 + hour * 3600) ^ (pid * 1664525u);
}

static uint32_t mix_seed(uint32_t base, int salt) {
	uint32_t value = base ^ (uint32_t)(salt * 2654435761u);
	return value != 0 ? value : (uint32_t)salt;
}

static uint32_t next_random(uint32_t *state) {
	uint32_t current = (*state == 0) ? 0xA5366D : *state;
	current = current * 1664525u + 1013904223u;
	*state = current;
	return current;
}

static void busy_wait(uint32_t iterations) {
	volatile uint32_t sink = 0;
	for (uint32_t i = 0; i < iterations; i++) {
		sink ^= i;
		if ((i & (BUSY_WAIT_YIELD_STEP - 1)) == 0) {
			yield();
		}
	}
	(void)sink;
}

static void random_busy_wait(uint32_t *state) {
	uint32_t value = next_random(state);
	uint32_t iterations = BUSY_WAIT_BASE + (value % BUSY_WAIT_JITTER);
	busy_wait(iterations);
}

static int writer_task(int argc, char **argv) {
	if (argc < 3) {
		return -1;
	}

	int writer_id = 0;
	if (sscanf(argv[1], "%d", &writer_id) != 1 || writer_id < 0) {
		return -1;
	}

	MVarContext *ctx = string_to_context(argv[2]);
	if (ctx == NULL || writer_id >= ctx->writer_count) {
		return -1;
	}

	uint32_t *seed = &ctx->writer_seeds[writer_id];
	char symbol = writer_symbol_for(writer_id);

	while (1) {
		random_busy_wait(seed);
		semWait(ctx->sem_empty);
		ctx->slot_value = symbol;
		semPost(ctx->sem_full);
	}

	return 0;
}

static int reader_task(int argc, char **argv) {
	if (argc < 3) {
		return -1;
	}

	int reader_id = 0;
	if (sscanf(argv[1], "%d", &reader_id) != 1 || reader_id < 0) {
		return -1;
	}

	MVarContext *ctx = string_to_context(argv[2]);
	if (ctx == NULL || reader_id >= ctx->reader_count) {
		return -1;
	}

	uint32_t *seed = &ctx->reader_seeds[reader_id];
	const char *color = reader_color_for(reader_id);

	while (1) {
		random_busy_wait(seed);
		semWait(ctx->sem_full);
		char value = ctx->slot_value;
		semPost(ctx->sem_empty);
		printf("%s%c\e[0m", color, value);
		yield();
	}

	return 0;
}

static int supervisor_task(int argc, char **argv) {
	if (argc < 2) {
		return -1;
	}

	MVarContext *ctx = string_to_context(argv[1]);
	if (ctx == NULL) {
		return -1;
	}

	for (int i = 0; i < ctx->writer_count; i++) {
		if (ctx->writer_pids[i] > 0) {
			waitPid(ctx->writer_pids[i]);
		}
	}

	for (int i = 0; i < ctx->reader_count; i++) {
		if (ctx->reader_pids[i] > 0) {
			waitPid(ctx->reader_pids[i]);
		}
	}

	if (ctx->sem_empty != NULL) {
		semDestroy(ctx->sem_empty);
	}
	if (ctx->sem_full != NULL) {
		semDestroy(ctx->sem_full);
	}

	free_context_memory(ctx);
	return 0;
}

static MVarContext *create_context(int writers, int readers) {
	MVarContext *ctx = myMalloc(sizeof(MVarContext));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->sem_empty = NULL;
	ctx->sem_full = NULL;
	ctx->slot_value = 0;
	ctx->writer_count = writers;
	ctx->reader_count = readers;
	ctx->writer_seeds = writers > 0 ? myMalloc(sizeof(uint32_t) * writers) : NULL;
	ctx->reader_seeds = readers > 0 ? myMalloc(sizeof(uint32_t) * readers) : NULL;
	ctx->writer_pids = writers > 0 ? myMalloc(sizeof(int32_t) * writers) : NULL;
	ctx->reader_pids = readers > 0 ? myMalloc(sizeof(int32_t) * readers) : NULL;

	if ((writers > 0 && (ctx->writer_seeds == NULL || ctx->writer_pids == NULL)) ||
	    (readers > 0 && (ctx->reader_seeds == NULL || ctx->reader_pids == NULL))) {
		free_context_memory(ctx);
		return NULL;
	}

	for (int i = 0; i < writers; i++) {
		ctx->writer_pids[i] = -1;
	}
	for (int i = 0; i < readers; i++) {
		ctx->reader_pids[i] = -1;
	}

	return ctx;
}

static void free_context_memory(MVarContext *ctx) {
	if (ctx == NULL) {
		return;
	}

	if (ctx->writer_seeds != NULL) {
		myFree(ctx->writer_seeds);
		ctx->writer_seeds = NULL;
	}
	if (ctx->reader_seeds != NULL) {
		myFree(ctx->reader_seeds);
		ctx->reader_seeds = NULL;
	}
	if (ctx->writer_pids != NULL) {
		myFree(ctx->writer_pids);
		ctx->writer_pids = NULL;
	}
	if (ctx->reader_pids != NULL) {
		myFree(ctx->reader_pids);
		ctx->reader_pids = NULL;
	}

	myFree(ctx);
}

static void pointer_to_string(void *ptr, char *buffer, int capacity) {
	if (buffer == NULL || capacity <= 0) {
		return;
	}

	const char hex_digits[] = "0123456789ABCDEF";
	uintptr_t value = (uintptr_t)ptr;
	int index = 0;

	if (capacity > 2) {
		buffer[index++] = '0';
		buffer[index++] = 'x';
	}

	int started = 0;
	for (int shift = (int)(sizeof(uintptr_t) * 8) - 4; shift >= 0 && index < capacity - 1; shift -= 4) {
		uint8_t nibble = (value >> shift) & 0xF;
		if (!started && nibble == 0 && shift > 0) {
			continue;
		}
		started = 1;
		buffer[index++] = hex_digits[nibble];
	}

	if (!started && index < capacity - 1) {
		buffer[index++] = '0';
	}

	buffer[index] = '\0';
}

static MVarContext *string_to_context(const char *value) {
	if (value == NULL) {
		return NULL;
	}

	uintptr_t result = 0;
	int i = 0;

	if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		i = 2;
	}

	while (value[i] != 0) {
		char c = value[i];
		uint8_t digit = 0;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = 10 + c - 'a';
		} else if (c >= 'A' && c <= 'F') {
			digit = 10 + c - 'A';
		} else {
			return NULL;
		}
		result = (result << 4) | digit;
		i++;
	}

	return (MVarContext *)result;
}

static void build_number_string(int value, char *buffer, int capacity) {
	if (buffer == NULL || capacity <= 0) {
		return;
	}

	int index = 0;
	if (value == 0) {
		if (capacity > 1) {
			buffer[index++] = '0';
		}
	} else {
		char temp[NUMBER_BUFFER_LEN];
		int t = 0;
		while (value > 0 && t < NUMBER_BUFFER_LEN - 1) {
			temp[t++] = '0' + (value % 10);
			value /= 10;
		}
		while (t > 0 && index < capacity - 1) {
			buffer[index++] = temp[--t];
		}
	}
	buffer[index] = '\0';
}

static void build_process_name(const char *prefix, int index, char *buffer, int capacity) {
	if (buffer == NULL || capacity <= 0) {
		return;
	}

	int pos = 0;
	if (prefix != NULL) {
		while (prefix[pos] != 0 && pos < capacity - 1) {
			buffer[pos] = prefix[pos];
			pos++;
		}
	}

	char suffix[NUMBER_BUFFER_LEN];
	build_number_string(index, suffix, sizeof(suffix));

	int suffix_pos = 0;
	while (suffix[suffix_pos] != 0 && pos < capacity - 1) {
		buffer[pos++] = suffix[suffix_pos++];
	}

	buffer[pos] = '\0';
}

static void build_sem_name(const char *prefix, const char *suffix, char *buffer, int capacity) {
	if (buffer == NULL || capacity <= 0) {
		return;
	}

	int pos = 0;
	if (prefix != NULL) {
		while (prefix[pos] != 0 && pos < capacity - 1) {
			buffer[pos] = prefix[pos];
			pos++;
		}
	}

	int i = 0;
	if (suffix != NULL) {
		while (suffix[i] != 0 && pos < capacity - 1) {
			buffer[pos++] = suffix[i++];
		}
	}

	buffer[pos] = '\0';
}

static char writer_symbol_for(int index) {
	if (index < 26) {
		return 'A' + index;
	}
	index -= 26;
	if (index < 26) {
		return 'a' + index;
	}
	index -= 26;
	return '0' + (index % 10);
}

static const char *reader_color_for(int index) {
	if (READER_COLOR_COUNT == 0) {
		return "\e[0m";
	}
	return reader_colors[index % READER_COLOR_COUNT];
}

static void kill_spawned_group(int32_t *pids, int count) {
	if (pids == NULL) {
		return;
	}

	for (int i = 0; i < count; i++) {
		if (pids[i] > 0) {
			kill(pids[i]);
		}
	}
}
