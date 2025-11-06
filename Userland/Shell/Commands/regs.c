#include "commands.h"

int _regs(int argc, char *argv[]) {
	if (argc > 1) {
		perror("Usage: regs\n");
		return 1;
	}
	const static char *register_names[] = {"rax", "rbx", "rcx", "rdx", "rbp", "rdi", "rsi", "r8 ", "r9 ",
										   "r10", "r11", "r12", "r13", "r14", "r15", "rsp", "rip", "rflags"};

	int64_t registers[18];

	uint8_t aux = getRegisterSnapshot(registers);

	if (aux == 0) {
		perror("No register snapshot available\n");
		return 1;
	}

	printf("Latest register snapshot:\n");

	for (int i = 0; i < 18; i++) {
		printf("\e[0;34m%s\e[0m: %x\n", register_names[i], registers[i]);
	}

	return 0;
}