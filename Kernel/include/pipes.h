#ifndef PIPES_H
#define PIPES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PIPES 64
#define PIPE_BUFFER_SIZE (1024 * 8) // 8K buffer size
#define STDIN 0
#define STDOUT 1
typedef struct pipeCDT * pipeADT;

int initPipes(void);
int openPipe(void);
int closePipe(int pipeID);
int readPipe(int pipeID, uint8_t * buffer, int size);
int writePipe(int pipeID, uint8_t * buffer, int size);


#endif