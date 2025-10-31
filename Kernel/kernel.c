#include <stdint.h>
#include <string.h>
#include <lib.h>
#include <moduleLoader.h>
#include <video.h>
#include <idtLoader.h>
#include <fonts.h>
#include <syscallDispatcher.h>
#include <sound.h>
#include <memory.h>
#include <scheduler.h>
#include <panic.h>
#include "time.h"

// extern uint8_t text;
// extern uint8_t rodata;
// extern uint8_t data;
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;

static const uint64_t PageSize = 0x1000;

static void * const shellModuleAddress = (void *)0x400000;
static void * const snakeModuleAddress = (void *)0x500000;

typedef int (*EntryPoint)();


void clearBSS(void * bssAddress, uint64_t bssSize){
	memset(bssAddress, 0, bssSize);
}

void * getStackBase() {
	return (void*)(
		(uint64_t)&endOfKernel
		+ PageSize * 8				//The size of the stack itself, 32KiB
		- sizeof(uint64_t)			//Begin at the top of the stack
	);
}

void * initializeKernelBinary(){
	void * moduleAddresses[] = {
		shellModuleAddress,
		snakeModuleAddress,
	};

	loadModules(&endOfKernelBinary, moduleAddresses);

	clearBSS(&bss, &endOfKernel - &bss);

	return getStackBase();
}

void processA(){
	while(1){
		print("Hello, kernel here!\n");
		sleep(1);
	}
	
}

void processB(){
	while(1){
		print("Process B, kernel here!\n");
		sleep(1);
	}
}

int main(){	
	load_idt();
	initMemory();
	initPCBTable();
	initScheduler();
	setFontSize(2);

	char ** argv = myMalloc(sizeof(char *) * 2);
	argv[0] = "shell";
	argv[1] = NULL;
	createProcess(shellModuleAddress, 1, argv, MID_PRIORITY, 1);

	// createProcess(processA, 0, NULL, MID_PRIORITY, -1);
	// createProcess(processB, 0, NULL, MID_PRIORITY, -1);

	//((EntryPoint)shellModuleAddress)();

	_sti();
	

	__builtin_unreachable();

	return 0;
}

