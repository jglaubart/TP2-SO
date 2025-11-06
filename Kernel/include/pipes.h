#ifndef PIPES_H
#define PIPES_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PIPES 64
#define PIPE_BUFFER_SIZE (1024 * 8) // 8K buffer size

#define READ_FD 0
#define WRITE_FD 1
#define PIPE_FD_COUNT 2

#define STDIN READ_FD
#define STDOUT WRITE_FD

typedef struct pipeCDT * pipeADT;

typedef enum {
    PIPE_ENDPOINT_NONE = 0,
    PIPE_ENDPOINT_CONSOLE,
    PIPE_ENDPOINT_PIPE,
} PipeEndpointType;

typedef enum {
    PIPE_ROLE_NONE = 0,
    PIPE_ROLE_READER,
    PIPE_ROLE_WRITER,
} PipeEndpointRole;

typedef struct {
    PipeEndpointType type;
    int pipeID; // valid only when type == PIPE_ENDPOINT_PIPE
    PipeEndpointRole role;
} PipeEndpoint;

void initPipes(void);
int openPipe(void);
int closePipe(int pipeID);
int readPipe(int pipeID, uint8_t * buffer, int size);
int writePipe(int pipeID, uint8_t * buffer, int size);

int pipeRetain(int pipeID, PipeEndpointRole role);
int pipeRelease(int pipeID, PipeEndpointRole role);

void pipeResetEndpoints(PipeEndpoint endpoints[PIPE_FD_COUNT]);
int pipeSetReadTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID);
int pipeSetWriteTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID);
int pipeReadEndpoint(PipeEndpoint *endpoint, uint8_t *buffer, int size);
int pipeWriteEndpoint(PipeEndpoint *endpoint, const uint8_t *buffer, int size);

#endif
