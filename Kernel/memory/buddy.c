#include <stddef.h>
#include <stdint.h>
#include <memory.h>

#define MIN_BLOCK_ORDER 5
#define MAX_BLOCK_ORDER 18
#define TOTAL_HEAP_SIZE (1u << MAX_BLOCK_ORDER)
#define DEPTH_LEVELS (MAX_BLOCK_ORDER - MIN_BLOCK_ORDER + 1)
#define TOTAL_NODES ((1u << DEPTH_LEVELS) - 1)

typedef enum {
    STATUS_AVAILABLE = 0,
    STATUS_DIVIDED,
    STATUS_OCCUPIED
} BlockStatus;

typedef struct TreeNode {
    struct TreeNode *parent_node;
    struct TreeNode *left_child;
    struct TreeNode *right_child;
    uint8_t block_order;
    BlockStatus status;
    uint8_t *mem_addr;
} TreeNode;

typedef struct AllocMetadata {
    TreeNode *tree_node;
} AllocMetadata;

static uint8_t heap[TOTAL_HEAP_SIZE];
static TreeNode tree_nodes[TOTAL_NODES];
static TreeNode *root_node = NULL;



void init_mm(void){}

void *malloc(size_t size){}

void free(void *ptr){}

void memstats(size_t *total, size_t *used, size_t *available){}