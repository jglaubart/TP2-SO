#include "memory.h"
#include <stdint.h>

// Configuración del heap
#define HEAP_SIZE (4096 * 128)  // 512K de Heap
#define BLOCK_SIZE 64             // Tamaño mínimo de bloque en bytes
#define NUM_BLOCKS (HEAP_SIZE / BLOCK_SIZE)  // Cantidad total de bloques
#define BITS_PER_BYTE 8           // Bits que tiene cada byte del bitmap

// Estructura para el bitmap
typedef struct {
    uint8_t bitmap[NUM_BLOCKS / BITS_PER_BYTE];  // Cada bit representa un bloque (1=usado, 0=libre)
    uint8_t heap[HEAP_SIZE];                      // El heap real donde se almacena la memoria
    int blocks_used;                           // Cantidad de bloques en uso
} MemoryManager;

// Instancia global del gestor de memoria
static MemoryManager mm;

// ==================== FUNCIONES AUXILIARES ====================

// Verifica si un bloque está ocupado
static int is_block_used(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    return (mm.bitmap[byte_index] >> bit_index) & 1;
}

// Marca un bloque como usado
static void mark_block_used(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    mm.bitmap[byte_index] |= (1 << bit_index);
}

// Marca un bloque como libre
static void mark_block_free(int block_index) {
    int byte_index = block_index / BITS_PER_BYTE;
    int bit_index = block_index % BITS_PER_BYTE;
    mm.bitmap[byte_index] &= ~(1 << bit_index);
}

// Encuentra bloques contiguos libres
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
    
    return NUM_BLOCKS;  // No se encontró espacio suficiente
}

// ==================== FUNCIONES PÚBLICAS ====================

// Inicializa el gestor de memoria
void initMemory(void) {
    // Limpia el bitmap (todos los bloques libres)
    for (int i = 0; i < NUM_BLOCKS / BITS_PER_BYTE; i++) {
        mm.bitmap[i] = 0;
    }
    
    // Limpia el heap
    for (int i = 0; i < HEAP_SIZE; i++) {
        mm.heap[i] = 0;
    }
    mm.blocks_used = 0;
}

// Reserva memoria
void * myMalloc(int size) {
    if (size == 0) {
        return NULL;
    }
    
    // Calcula cuántos bloques se necesitan
    int blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // Busca bloques contiguos libres
    int start_block = find_free_blocks(blocks_needed);
    
    if (start_block == NUM_BLOCKS) {
        return NULL;  // No hay memoria suficiente
    }
    
    // Marca los bloques como usados
    for (int i = 0; i < blocks_needed; i++) {
        mark_block_used(start_block + i);
    }
    
    mm.blocks_used += blocks_needed;
    
    // Retorna puntero al inicio del bloque en el heap
    return (void *)&mm.heap[start_block * BLOCK_SIZE];
}

// Libera memoria
void myFree(void *ptr) {
        if (ptr == NULL) {
        return;
    }
    
    // Calcula el índice del bloque inicial
    uint8_t *heap_start = mm.heap;
    int offset = (uint8_t *)ptr - heap_start;
    int start_block = offset / BLOCK_SIZE;
    
    // Verifica que el puntero sea válido
    if (start_block >= NUM_BLOCKS) {
        return;
    }
    
    // Libera bloques contiguos hasta encontrar uno libre
    int blocks_freed = 0;
    for (int i = start_block; i < NUM_BLOCKS && is_block_used(i); i++) {
        mark_block_free(i);
        blocks_freed++;
    }
    
    mm.blocks_used -= blocks_freed;
}

// Obtiene estadísticas de memoria
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
    
    return 1;
}
