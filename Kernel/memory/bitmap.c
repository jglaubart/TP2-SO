#include "memory.h"
#include <stdint.h>
#include <string.h>

#define HEAP_SIZE (4096 * 128)  // 512K Heap
#define BLOCK_SIZE 64             // Minimum block size in bytes
#define NUM_BLOCKS (HEAP_SIZE / BLOCK_SIZE)  // Total number of blocks
#define BITS_PER_BYTE 8           // Bits per byte in the bitmap
#define BITMAP_NUM_BYTES ((NUM_BLOCKS + BITS_PER_BYTE - 1) / BITS_PER_BYTE)
#define BLOCK_CONTINUATION 0xFFFFu

// Bitmap and heap structure
typedef struct {
    uint8_t bitmap[BITMAP_NUM_BYTES];          // Each bit represents a block (1=used, 0=free)
    uint8_t heap[HEAP_SIZE];                      // Actual heap where memory is stored
    uint16_t allocation_map[NUM_BLOCKS];          // Blocks occupied by reservation (only for the initial block)
    int blocks_used;                           // Number of blocks in use
} MemoryManager;

// Global memory manager instance
static MemoryManager mm;

// ==================== Helper Functions ====================

// Checks if a block is occupied
static int is_block_used(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    return (mm.bitmap[byte_index] >> bit_index) & 1;
}

// Marks a block as used
static void mark_block_used(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    mm.bitmap[byte_index] |= (1 << bit_index);
}

// Marks a block as free
static void mark_block_free(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    mm.bitmap[byte_index] &= ~(1 << bit_index);
}

// Finds contiguous free blocks
static int find_free_blocks(int num_blocks_needed) {
    int consecutive_free = 0;
    int start_block = 0;
    
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!is_block_used(i)) {
            if (consecutive_free == 0) {
                start_block = i;
            }
            consecutive_free++;
            
            if (consecutive_free == num_blocks_needed) {
                return start_block;
            }
        } else {
            consecutive_free = 0;
        }
    }
    
    return NUM_BLOCKS;
}

// ==================== Public Functions ====================
void initMemory(void) {
    memset(mm.bitmap, 0, sizeof(mm.bitmap));
    memset(mm.allocation_map, 0, sizeof(mm.allocation_map));
    memset(mm.heap, 0, sizeof(mm.heap));
    mm.blocks_used = 0;
}

void * myMalloc(int size) {
    if (size == 0) {
        return NULL;
    }
    
    int blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed <= 0 || blocks_needed > NUM_BLOCKS || blocks_needed >= BLOCK_CONTINUATION) {
        return NULL;
    }
    
    int start_block = find_free_blocks(blocks_needed);
    
    if (start_block == NUM_BLOCKS) {
        return NULL;  // Not enough memory available
    }
    
    for (int i = 0; i < blocks_needed; i++) {
        mark_block_used(start_block + i);
    }
    
    mm.blocks_used += blocks_needed;
    mm.allocation_map[start_block] = (uint16_t)blocks_needed;
    for (int i = 1; i < blocks_needed; i++) {
        mm.allocation_map[start_block + i] = BLOCK_CONTINUATION;
    }

    // Returns a pointer to the beginning of the block in the heap
    return (void *)&mm.heap[start_block * BLOCK_SIZE];
}

void myFree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    uint8_t *ptr_byte = (uint8_t *)ptr;
    uint8_t *heap_start = mm.heap;
    uint8_t *heap_end = mm.heap + HEAP_SIZE;

    if (ptr_byte < heap_start || ptr_byte >= heap_end) {
        return;
    }

    int offset = ptr_byte - heap_start;
    if (offset % BLOCK_SIZE != 0) {
        return;
    }

    int start_block = offset / BLOCK_SIZE;
    uint16_t blocks_to_free = mm.allocation_map[start_block];
    if (blocks_to_free == 0 || blocks_to_free == BLOCK_CONTINUATION) {
        return;
    }

    // Frees contiguous blocks until a free one is found
    for (int i = 0; i < blocks_to_free && (start_block + i) < NUM_BLOCKS; i++) {
        mark_block_free(start_block + i);
        mm.allocation_map[start_block + i] = 0;
    }
    
    if (mm.blocks_used >= (int)blocks_to_free) {
        mm.blocks_used -= (int)blocks_to_free;
    } else {
        mm.blocks_used = 0;
    }
}

void memstats(int *total, int *used, int *available) {
    if (total != NULL) {
        *total = HEAP_SIZE;
    }
    
    if (used != NULL) {
        *used = mm.blocks_used * BLOCK_SIZE;
    }
    
    if (available != NULL) {
        *available = HEAP_SIZE - (mm.blocks_used * BLOCK_SIZE);
    }
}

int isValidHeapPtr(void *ptr) {
    if (ptr == NULL) {
        return 0;
    }
    
    uint8_t *ptr_byte = (uint8_t *)ptr;
    uint8_t *heap_start = mm.heap;
    uint8_t *heap_end = mm.heap + HEAP_SIZE;
    
    // Check if pointer is within heap bounds
    if (ptr_byte < heap_start || ptr_byte >= heap_end) {
        return 0;
    }
    
    // Calculate the offset from the heap start
    int offset = ptr_byte - heap_start;
    
    // Check if the pointer is aligned to a block boundary
    // (myMalloc only returns pointers at block boundaries)
    if (offset % BLOCK_SIZE != 0) {
        return 0;
    }
    
    // Calculate the block index
    int block_index = offset / BLOCK_SIZE;
    
    // Verify the block is actually marked as used
    if (!is_block_used(block_index)) {
        return 0;
    }

    uint16_t blocks_tracked = mm.allocation_map[block_index];
    if (blocks_tracked == 0 || blocks_tracked == BLOCK_CONTINUATION) {
        return 0;
    }
    
    return 1;
}
