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

// Initializes pipe structures and resets state.
void initPipes(void);
// Creates a new pipe and returns its identifier.
int openPipe(void);
// Closes the specified pipe and frees resources.
int closePipe(int pipeID);
// Reads up to size bytes from the given pipe into buffer.
int readPipe(int pipeID, uint8_t * buffer, int size);
// Writes up to size bytes from buffer into the given pipe.
int writePipe(int pipeID, uint8_t * buffer, int size);

// Increments reader or writer references on a pipe.
int pipeRetain(int pipeID, PipeEndpointRole role);
// Decrements reader or writer references and closes when unused.
int pipeRelease(int pipeID, PipeEndpointRole role);

// Resets an endpoint array to the default disconnected state.
void pipeResetEndpoints(PipeEndpoint endpoints[PIPE_FD_COUNT]);
// Configures the read endpoint target.
int pipeSetReadTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID);
// Configures the write endpoint target.
int pipeSetWriteTarget(PipeEndpoint endpoints[PIPE_FD_COUNT], PipeEndpointType type, int pipeID);
// Reads through the provided endpoint abstraction.
int pipeReadEndpoint(PipeEndpoint *endpoint, uint8_t *buffer, int size);
// Writes through the provided endpoint abstraction.
int pipeWriteEndpoint(PipeEndpoint *endpoint, const uint8_t *buffer, int size);

#endif
