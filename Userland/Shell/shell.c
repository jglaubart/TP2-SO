// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "shell.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "commands.h"
#include <sys.h>
#include <syscalls.h>

#ifdef ANSI_4_BIT_COLOR_SUPPORT
#include <ansiColors.h>
#endif

#define SHELL_PROMPT "\e[0mshell \e[0;32m$\e[0m "
#define INPUT_CAPACITY 1024
#define HISTORY_LIMIT 10
#define MAX_PIPE_SEGMENTS 8
#define MAX_ARGUMENTS 16
#define SHELL_CTRL_K_CHAR 0x0B

typedef struct {
	Command *command;
	char *argv[MAX_ARGUMENTS];
	int argc;
	uint8_t background;
	int32_t pid;
} CommandInvocation;

static char input_line[INPUT_CAPACITY];
static int input_length = 0;

static char history_entries[HISTORY_LIMIT][INPUT_CAPACITY];
static int history_size = 0;
static int history_cursor = 0;
static int history_replay_remaining = 0;

static int exit_requested = 0;
static int exit_status = 0;

static void show_prompt(void);
static int capture_line(void);
static void reset_input_buffer(void);
static void remember_command(const char *line);
static void copy_line(char *dest, const char *src, int capacity);
static Command *find_command(const char *name);
static int parse_commands(char *line, CommandInvocation *calls, int *count, char **unknown);
static int run_pipeline(CommandInvocation *calls, int count, int pipeline_background);
static void reset_invocation(CommandInvocation *invocation);
static int is_whitespace_char(char c);
static int strings_match(const char *a, const char *b);

static void recall_previous(enum REGISTERABLE_KEYS scancode);
static void recall_next(enum REGISTERABLE_KEYS scancode);
static void handle_backspace(enum REGISTERABLE_KEYS scancode);
static void handle_mvar_kill_hotkey(void);

int history(int argc, char **argv);
int _getPid(int argc, char **argv);

Command commands[] = {
	{.name = "block", .function = _block, .description = "Toggles a process between ready and blocked: block <pid>", .is_builtin = 0},
	{.name = "cat", .function = _cat, .description = "Prints stdin exactly as received", .is_builtin = 0},
    {.name = "clear", .function = _clear, .description = "Clears the screen", .is_builtin = 0},
	{.name = "divzero", .function = _exception_divzero, .description = "Generates a division by zero exception", .is_builtin = 0},
	{.name = "echo", .function = _echo, .description = "Prints the provided arguments", .is_builtin = 0},
	{.name = "filter", .function = _filter, .description = "Removes vowels from stdin", .is_builtin = 0},
	{.name = "font", .function = _font, .description = "Adjusts font size", .is_builtin = 0},
    {.name = "getpid", .function = _getPid, .description = "Gets the current process ID", .is_builtin = 1},
	{.name = "help", .function = _help, .description = "Shows the available commands", .is_builtin = 0},
	{.name = "history", .function = history, .description = "Prints the command history", .is_builtin = 1},
	{.name = "invop", .function = _exception_invop, .description = "Generates an invalid opcode exception", .is_builtin = 0},
	{.name = "kill", .function = _shell_kill, .description = "Terminates the provided PID", .is_builtin = 0},
	{.name = "loop", .function = _loop, .description = "Prints a message every specified ms", .is_builtin = 0},
	{.name = "man", .function = _man, .description = "Shows the manual for a command", .is_builtin = 0},
	{.name = "mem", .function = _mem_stats, .description = "Displays memory statistics", .is_builtin = 0},
    {.name = "mvar", .function = _mvar, .description = "Creates a multi-variable process", .is_builtin = 0},
    {.name = "mvar-kill", .function = _mvar_close, .description = "Kills mvar processes (Ctrl+K)", .is_builtin = 0},
	{.name = "nice", .function = _nice, .description = "Changes a process priority: nice <pid> <priority>", .is_builtin = 0},
	{.name = "ps", .function = _ps, .description = "Lists active processes", .is_builtin = 0},
	{.name = "regs", .function = _regs, .description = "Prints the last register snapshot", .is_builtin = 0},
	{.name = "snake", .function = _snake, .description = "Launches the snake game", .is_builtin = 0},
	{.name = "test_mm", .function = _test_mm, .description = "Stress tests the memory manager: test_mm <max_memory>", .is_builtin = 0},
	{.name = "test_prio", .function = _test_prio, .description = "Spawns processes with different priorities: test_prio <max_value>", .is_builtin = 0},
	{.name = "test_processes", .function = _test_processes, .description = "Creates and kills processes randomly: test_processes <max_processes>", .is_builtin = 0},
	{.name = "test_sync", .function = _test_sync, .description = "Synchronization race test: test_sync <iterations> <use_semaphore:0|1>", .is_builtin = 0},
	{.name = "test_wait_children", .function = _test_wait_children, .description = "Spawns children and waits for all: test_wait_children [child_count]", .is_builtin = 0},
	{.name = "time", .function = _time, .description = "Displays the current time", .is_builtin = 0},
	{.name = "wc", .function = _wc, .description = "Counts stdin lines", .is_builtin = 0},
};

const int commands_size = sizeof(commands) / sizeof(commands[0]);

int main(void) {
	registerKey(KP_UP_KEY, recall_previous);
	registerKey(KP_DOWN_KEY, recall_next);
	registerKey(BACKSPACE_KEY, handle_backspace);

	while (!exit_requested) {
		show_prompt();
		history_cursor = history_size;

		if (capture_line() < 0) {
			reset_input_buffer();
			continue;
		}

		char original_line[INPUT_CAPACITY];
		copy_line(original_line, input_line, INPUT_CAPACITY);

		int only_whitespace = 1;
		for (int i = 0; original_line[i] != '\0'; i++) {
			if (!is_whitespace_char(original_line[i])) {
				only_whitespace = 0;
				break;
			}
		}

		if (only_whitespace) {
			reset_input_buffer();
			continue;
		}

		CommandInvocation pipeline[MAX_PIPE_SEGMENTS];
		for (int i = 0; i < MAX_PIPE_SEGMENTS; i++) {
			reset_invocation(&pipeline[i]);
		}

		int command_count = 0;
		char *unknown = NULL;
		int parse_status = parse_commands(input_line, pipeline, &command_count, &unknown);

		if (parse_status != 0) {
			switch (parse_status) {
				case -1:
					fprintf(FD_STDERR, "shell: too many commands chained with '|'\n");
					break;
				case -2:
					if (unknown != NULL) {
						fprintf(FD_STDERR, "shell: too many arguments for '%s'\n", unknown);
					} else {
						fprintf(FD_STDERR, "shell: too many arguments supplied\n");
					}
					break;
				case -3:
					fprintf(FD_STDERR, "shell: syntax error near '|'\n");
					break;
				case -6:
					fprintf(FD_STDERR, "shell: cannot pipe more than two processes at once\n");
					break;
				case -4:
					fprintf(FD_STDERR, "shell: command not found: %s\n", unknown != NULL ? unknown : "");
					break;
				default:
					break;
			}
			reset_input_buffer();
			continue;
		}

		if (command_count == 0) {
			reset_input_buffer();
			continue;
		}

		remember_command(original_line);
		history_cursor = history_size;

		int last_background = pipeline[command_count - 1].background;
		int invalid_background = 0;
		for (int i = 0; i < command_count - 1; i++) {
			if (pipeline[i].background) {
				invalid_background = 1;
				break;
			}
		}

		if (invalid_background) {
			fprintf(FD_STDERR, "shell: only the last command in a pipeline can request background execution\n");
			reset_input_buffer();
			continue;
		}

		int builtin_in_pipeline = 0;
		for (int i = 0; i < command_count; i++) {
			if (pipeline[i].command->is_builtin) {
				if (command_count > 1) {
					builtin_in_pipeline = 1;
					break;
				}
				if (pipeline[i].background) {
					fprintf(FD_STDERR, "shell: builtin commands cannot run in background\n");
					builtin_in_pipeline = 1;
					break;
				}
			}
		}

		if (builtin_in_pipeline) {
			reset_input_buffer();
			continue;
		}

		if (command_count == 1 && pipeline[0].command->is_builtin) {
			pipeline[0].command->function(pipeline[0].argc, pipeline[0].argv);
		} else {
			if (run_pipeline(pipeline, command_count, last_background) != 0) {
				fprintf(FD_STDERR, "shell: failed to launch commands\n");
			}
		}

		reset_input_buffer();
	}

	return exit_status;
}

static void show_prompt(void) {
	printf("%s", SHELL_PROMPT);
}

static void reset_input_buffer(void) {
	input_line[0] = '\0';
	input_length = 0;
	history_replay_remaining = 0;
}

static int capture_line(void) {
	int ch = 0;
	input_length = 0;
	history_replay_remaining = 0;

	while (input_length < INPUT_CAPACITY - 1) {
		ch = getchar();

		if (ch == SHELL_CTRL_K_CHAR) {
			handle_mvar_kill_hotkey();
			continue;
		}

		if (ch == -1) {
			if (input_length == 0) {
				putchar('\n');
				return -1;
			}
			input_line[input_length] = '\0';
			//putchar('\n');
			return input_length;
		}

		if (ch == '\n') {
			break;
		}

		if (history_replay_remaining > 0) {
			history_replay_remaining--;
			continue;
		}

		if (ch < 0) {
			continue;
		}
		input_line[input_length++] = (char)ch;
	}

	if (ch != '\n') {
		fprintf(FD_STDERR, "\nShell input overflow\n");
		do {
			ch = getchar();
		} while (ch != '\n' && ch >= 0);
		return -1;
	}

	putchar('\n');
	input_line[input_length] = '\0';
	return input_length;
}

static void remember_command(const char *line) {
	if (line == NULL || *line == '\0') {
		return;
	}
	if (history_size < HISTORY_LIMIT) {
		copy_line(history_entries[history_size], line, INPUT_CAPACITY);
		history_size++;
	} else {
		for (int i = 1; i < HISTORY_LIMIT; i++) {
			copy_line(history_entries[i - 1], history_entries[i], INPUT_CAPACITY);
		}
		copy_line(history_entries[HISTORY_LIMIT - 1], line, INPUT_CAPACITY);
	}
}

static Command *find_command(const char *name) {
	if (name == NULL) {
		return NULL;
	}
	for (int i = 0; i < commands_size; i++) {
		if (strings_match(commands[i].name, name)) {
			return &commands[i];
		}
	}
	return NULL;
}

static int parse_commands(char *line, CommandInvocation *calls, int *count, char **unknown) {
	if (line == NULL || calls == NULL || count == NULL) {
		return -5;
	}

	*unknown = NULL;
	int stages = 0;
	int pipe_count = 0;
	char *segment_ptr = line;

	while (segment_ptr != NULL) {
		if (stages >= MAX_PIPE_SEGMENTS) {
			return -1;
		}

		char *segment_start = segment_ptr;
		char *pipe_pos = NULL;
		for (char *scan = segment_start; scan != NULL && *scan != '\0'; scan++) {
			if (*scan == '|') {
				pipe_pos = scan;
				break;
			}
		}

		char *next_segment = NULL;
		if (pipe_pos != NULL) {
			if (pipe_count >= 1) {
				return -6;
			}
			pipe_count++;
			*pipe_pos = '\0';
			next_segment = pipe_pos + 1;
		}

		while (*segment_start != '\0' && is_whitespace_char(*segment_start)) {
			segment_start++;
		}

		char *segment_end = segment_start + strlen(segment_start);
		while (segment_end > segment_start && is_whitespace_char(*(segment_end - 1))) {
			segment_end--;
		}
		*segment_end = '\0';

		if (*segment_start == '\0') {
			return -3;
		}

		CommandInvocation *current = &calls[stages];
		reset_invocation(current);

		char *token = strtok(segment_start, " ");
		while (token != NULL) {
			if (current->argc >= MAX_ARGUMENTS - 1) {
				*unknown = current->argv[0];
				return -2;
			}
			current->argv[current->argc++] = token;
			token = strtok(NULL, " ");
		}

		if (current->argc == 0) {
			return -3;
		}

		char *last = current->argv[current->argc - 1];
		size_t last_len = strlen(last);
		if (strcmp(last, "&") == 0) {
			current->argv[current->argc - 1] = NULL;
			current->argc--;
			current->background = 1;
		} else if (last_len > 0 && last[last_len - 1] == '&') {
			current->background = 1;
			while (last_len > 0 && last[last_len - 1] == '&') {
				last[--last_len] = '\0';
			}
			if (last_len == 0) {
				current->argc--;
			}
		}

		if (current->argc == 0) {
			return -3;
		}

		current->argv[current->argc] = NULL;
		current->command = find_command(current->argv[0]);
		if (current->command == NULL) {
			*unknown = current->argv[0];
			return -4;
		}

		current->pid = -1;

		stages++;
		segment_ptr = next_segment;
	}

	*count = stages;
	return 0;
}

static int run_pipeline(CommandInvocation *calls, int count, int pipeline_background) {
	int32_t spawned_pids[MAX_PIPE_SEGMENTS];
	int pid_count = 0;
	int pending_pipe = -1;

	for (int i = 0; i < count; i++) {
		CommandInvocation *current = &calls[i];
		int is_last = (i == count - 1);

		if (current->command == NULL || current->command->is_builtin) {
			continue;
		}

		int pipefd[PIPE_FD_COUNT] = {-1, -1};
		int next_pipe = -1;
		if (!is_last) {
			if (openPipe(pipefd) != 0) {
				fprintf(FD_STDERR, "shell: unable to create pipe\n");
				return -1;
			}
			next_pipe = pipefd[READ_FD];
		}

		int input_type = (pending_pipe >= 0) ? PIPE_ENDPOINT_PIPE : PIPE_ENDPOINT_CONSOLE;
		int input_target = (pending_pipe >= 0) ? pending_pipe : -1;
		if (setFdTarget(READ_FD, input_type, input_target) != 0) {
			fprintf(FD_STDERR, "shell: unable to configure stdin\n");
			setFdTarget(READ_FD, PIPE_ENDPOINT_CONSOLE, -1);
			return -1;
		}

		int output_type = (next_pipe >= 0) ? PIPE_ENDPOINT_PIPE : PIPE_ENDPOINT_CONSOLE;
		int output_target = (next_pipe >= 0) ? next_pipe : -1;
		if (setFdTarget(WRITE_FD, output_type, output_target) != 0) {
			fprintf(FD_STDERR, "shell: unable to configure stdout\n");
			setFdTarget(READ_FD, PIPE_ENDPOINT_CONSOLE, -1);
			setFdTarget(WRITE_FD, PIPE_ENDPOINT_CONSOLE, -1);
			return -1;
		}

		uint8_t run_in_background = pipeline_background ? 1 : (is_last ? 0 : 1);
		int32_t pid = createProcess((void *)current->command->function, current->argc, (uint8_t **)current->argv, run_in_background);
		setFdTarget(READ_FD, PIPE_ENDPOINT_CONSOLE, -1);
		setFdTarget(WRITE_FD, PIPE_ENDPOINT_CONSOLE, -1);

		if (pid < 0) {
			fprintf(FD_STDERR, "shell: unable to create process for '%s'\n", current->command->name);
			return -1;
		}

		spawned_pids[pid_count++] = pid;
		current->pid = pid;
		pending_pipe = next_pipe;
	}

	if (!pipeline_background) {
		int upstream_signaled = 0;
		for (int i = pid_count - 1; i >= 0; i--) {
			int32_t pid = spawned_pids[i];
			if (pid <= 0) {
				continue;
			}
			waitPid(pid);
			if (!upstream_signaled && i == pid_count - 1) {
				for (int j = 0; j < i; j++) {
					if (spawned_pids[j] > 0) {
						kill(spawned_pids[j]);
					}
				}
				upstream_signaled = 1;
			}
		}
	}

	return 0;
}

static void copy_line(char *dest, const char *src, int capacity) {
	if (dest == NULL || src == NULL || capacity <= 0) {
		return;
	}
	int i = 0;
	while (i < capacity - 1 && src[i] != '\0') {
		dest[i] = src[i];
		i++;
	}
	dest[i] = '\0';
	i++;
	while (i < capacity) {
		dest[i++] = '\0';
	}
}

static void recall_previous(enum REGISTERABLE_KEYS scancode) {
	if (history_size == 0) {
		return;
	}
	if (history_cursor > 0) {
		history_cursor--;
	}
	clearInputBuffer();
	reset_input_buffer();
	const char *entry = history_entries[history_cursor];
	int len = strlen(entry);
	if (len > 0) {
		int max_copy = len < INPUT_CAPACITY - 1 ? len : INPUT_CAPACITY - 1;
		copy_line(input_line, entry, INPUT_CAPACITY);
		input_length = max_copy;
		input_line[input_length] = '\0';
		history_replay_remaining = input_length;
		sys_write(FD_STDIN, input_line, input_length);
	}
}

static void recall_next(enum REGISTERABLE_KEYS scancode) {
	if (history_size == 0) {
		return;
	}
	if (history_cursor < history_size - 1) {
		history_cursor++;
		clearInputBuffer();
		reset_input_buffer();
		const char *entry = history_entries[history_cursor];
		int len = strlen(entry);
		if (len > 0) {
			int max_copy = len < INPUT_CAPACITY - 1 ? len : INPUT_CAPACITY - 1;
			copy_line(input_line, entry, INPUT_CAPACITY);
			input_length = max_copy;
			input_line[input_length] = '\0';
			history_replay_remaining = input_length;
			sys_write(FD_STDIN, input_line, input_length);
		}
	} else {
		history_cursor = history_size;
		clearInputBuffer();
		reset_input_buffer();
	}
}

static void reset_invocation(CommandInvocation *invocation) {
	if (invocation == NULL) {
		return;
	}
	invocation->command = NULL;
	invocation->argc = 0;
	invocation->background = 0;
	invocation->pid = -1;
	for (int i = 0; i < MAX_ARGUMENTS; i++) {
		invocation->argv[i] = NULL;
	}
}

static int is_whitespace_char(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int strings_match(const char *a, const char *b) {
	if (a == NULL || b == NULL) {
		return 0;
	}
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return 0;
		}
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static void handle_backspace(enum REGISTERABLE_KEYS scancode) {
	if (input_length > 0) {
		input_length--;
		input_line[input_length] = '\0';
		if (history_replay_remaining > 0) {
			history_replay_remaining--;
		}
	}
}

static void handle_mvar_kill_hotkey(void) {
	putchar('\n');
	reset_input_buffer();
	char *argv[] = {"mvar-kill", NULL};
	_mvar_close(1, argv);
	show_prompt();
}

int history(int argc, char **argv) {
	if (argc != 1) {
		fprintf(FD_STDERR, "Usage: history\n");
		return 1;
	}

	for (int i = history_size - 1, idx = 0; i >= 0; i--, idx++) {
		printf("%d. %s\n", idx, history_entries[i]);
	}
	return 0;
}

int _getPid(int argc, char **argv) {
    int pid = getPid();
    printf("Current process ID: %d\n", pid);
    return 0;
}
