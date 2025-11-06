#ifndef _SYS_H_
#define _SYS_H_

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include <syscalls.h>

void startBeep(uint32_t nFrequence);
void stopBeep(void);
void setTextColor(uint32_t color);
void setBackgroundColor(uint32_t color);
uint8_t increaseFontSize(void);
uint8_t decreaseFontSize(void);
uint8_t setFontSize(uint8_t size);
void getDate(int * hour, int * minute, int * second);
void clearScreen(void);


void drawCircle(uint32_t color, long long int topleftX, long long int topLefyY, long long int diameter);
void drawRectangle(uint32_t color, long long int width_pixels, long long int height_pixels, long long int initial_pos_x, long long int initial_pos_y);
void fillVideoMemory(uint32_t hexColor);
int32_t exec(int32_t (*fnPtr)(void));
int32_t execProgram(int32_t (*fnPtr)(void));
void registerKey(enum REGISTERABLE_KEYS scancode, void (*fn)(enum REGISTERABLE_KEYS scancode));
void clearInputBuffer(void);
int getWindowWidth(void);
int getWindowHeight(void);
void sleep(uint32_t milliseconds);
int32_t getRegisterSnapshot(int64_t * registers);
int32_t getCharacterWithoutDisplay(void);

void * myMalloc(int size);
int32_t myFree(void * ptr);
int32_t mem(int * total, int * used, int * available);

int32_t getPid(void);
int32_t createProcess(void * function, uint64_t argc, uint8_t ** argv, uint8_t is_background);
int32_t unblock(int pid);
int32_t block(int pid);
int32_t kill(int pid);
int32_t nice(int pid, int newPriority);
int32_t waitPid(int pid);
int32_t waitChildren(void);
int32_t ps(ProcessInformation * processInfoTable);
int32_t yield(void);

void * semInit(const char *name, uint32_t initial_count);
int32_t semPost(void * sem);
int32_t semWait(void * sem);
int32_t semDestroy(void * sem);

int32_t openPipe(int pipefd[2]);
int32_t closePipe(int pipeID);
int32_t setFdTarget(int fd, int type, int pipeID);

#endif
