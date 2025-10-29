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


// FUNCIONES AUXILIARES 

static TreeNode *build_tree(int idx, int order, uint8_t *mem_base) {
    TreeNode *current = &tree_nodes[idx];
    current->parent_node = NULL;
    current->left_child = NULL;
    current->right_child = NULL;
    current->block_order = (uint8_t)order;
    current->status = STATUS_AVAILABLE;
    current->mem_addr = mem_base;

    if (order > MIN_BLOCK_ORDER) {
        int left_idx = (idx * 2) + 1;
        int right_idx = left_idx + 1;
        size_t half_size = get_block_size(order - 1);

        TreeNode *left_node = build_tree(left_idx, order - 1, mem_base);
        TreeNode *right_node = build_tree(right_idx, order - 1, mem_base + half_size);

        left_node->parent_node = current;
        right_node->parent_node = current;
        current->left_child = left_node;
        current->right_child = right_node;
    }

    return current;
}


static size_t count_available_bytes(const TreeNode *current) {
    if (current == NULL) {
        return 0;
    }

    if (current->status == STATUS_AVAILABLE) {
        return get_block_size(current->block_order);
    }

    if (current->status == STATUS_DIVIDED) {
        return count_available_bytes(current->left_child) + count_available_bytes(current->right_child);
    }

    return 0;
}


void init_mm(void) {
    root_node = build_tree(0, MAX_BLOCK_ORDER, heap);
}

void *malloc(size_t size){}

void free(void *ptr){}

void memstats(size_t *total, size_t *used, size_t *available) {
    if (root_node == NULL) {
        init_mm();
    }
    if (total != NULL) {
        *total = TOTAL_HEAP_SIZE;
    }
    size_t available_total = count_available_bytes(root_node);

    if (available != NULL) {
        *available = available_total;
    }
    if (used != NULL) {
        *used = TOTAL_HEAP_SIZE - available_total;
    }
}