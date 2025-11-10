// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <stddef.h>
#include <stdint.h>

#define MIN_BLOCK_ORDER 5
#define MAX_BLOCK_ORDER 19
#define TOTAL_HEAP_SIZE (1u << MAX_BLOCK_ORDER)
#define DEPTH_LEVELS (MAX_BLOCK_ORDER - MIN_BLOCK_ORDER + 1)
#define TOTAL_NODES ((1u << DEPTH_LEVELS) - 1)

typedef enum {
    STATUS_AVAILABLE = 0,
    STATUS_DIVIDED,
    STATUS_OCCUPIED
} BlockStatus;

typedef struct TreeNode {
    uint32_t offset;
    uint8_t block_order;
    uint8_t status;
} TreeNode;

static uint8_t heap[TOTAL_HEAP_SIZE];
static TreeNode tree_nodes[TOTAL_NODES];
static int allocator_initialized = 0;
static int total_free_bytes = TOTAL_HEAP_SIZE;


// ========== Helper Functions ========== 
// Returns the byte size of a block for the given order.
static int get_block_size(int order) {
    return (int)1u << order;
}
// Recursively builds the buddy tree starting from the given node.
static void build_tree(int idx, int order, uint32_t offset) {
    TreeNode *current = &tree_nodes[idx];
    current->offset = offset;
    current->block_order = (uint8_t)order;
    current->status = STATUS_AVAILABLE;

    if (order > MIN_BLOCK_ORDER) {
        int left_idx = (idx * 2) + 1;
        int right_idx = left_idx + 1;
        int half_size = get_block_size(order - 1);

        build_tree(left_idx, order - 1, offset);
        build_tree(right_idx, order - 1, offset + (uint32_t)half_size);
    }
}


// Determines the smallest order that can accommodate the requested size.
static int calculate_order(int size) {
    int order = MIN_BLOCK_ORDER;

    while (order <= MAX_BLOCK_ORDER && get_block_size(order) < size) {
        order++;
    }

    return order;
}

// Finds a suitable node and marks it as allocated.
static int find_and_allocate(int idx, int order) {
    TreeNode *current = &tree_nodes[idx];

    if (current->status == STATUS_OCCUPIED) {
        return -1;
    }

    if (current->block_order < order) {
        return -1;
    }

    if (current->block_order == order) {
        if (current->status == STATUS_AVAILABLE) {
            current->status = STATUS_OCCUPIED;
            return idx;
        }
        return -1;
    }

    int left_idx = (idx * 2) + 1;
    int right_idx = left_idx + 1;

    if (right_idx >= TOTAL_NODES) {
        return -1;
    }

    if (current->status == STATUS_AVAILABLE) {
        current->status = STATUS_DIVIDED;
    }

    int allocated = find_and_allocate(left_idx, order);
    if (allocated >= 0) {
        return allocated;
    }

    allocated = find_and_allocate(right_idx, order);
    if (allocated >= 0) {
        return allocated;
    }

    if (tree_nodes[left_idx].status == STATUS_AVAILABLE && tree_nodes[right_idx].status == STATUS_AVAILABLE) {
        current->status = STATUS_AVAILABLE;
    }

    return -1;
}

// Attempts to merge a freed node with its parents when possible.
static void merge_upwards(int idx) {
    while (idx > 0) {
        int parent_idx = (idx - 1) >> 1;
        int left_idx = (parent_idx * 2) + 1;
        int right_idx = left_idx + 1;

        if (right_idx >= TOTAL_NODES) {
            break;
        }

        if (tree_nodes[left_idx].status == STATUS_AVAILABLE && tree_nodes[right_idx].status == STATUS_AVAILABLE) {
            tree_nodes[parent_idx].status = STATUS_AVAILABLE;
            idx = parent_idx;
        } else {
            tree_nodes[parent_idx].status = STATUS_DIVIDED;
            break;
        }
    }
}

// Locates the tree node that matches the given heap offset.
static int locate_node_for_offset(int idx, uint32_t target_offset) {
    if (idx < 0 || idx >= TOTAL_NODES) {
        return -1;
    }

    TreeNode *node = &tree_nodes[idx];
    uint32_t node_offset = node->offset;
    int size = get_block_size(node->block_order);

    if (target_offset < node_offset || target_offset >= node_offset + (uint32_t)size) {
        return -1;
    }

    if (node->status == STATUS_OCCUPIED) {
        return (target_offset == node_offset) ? idx : -1;
    }

    if (node->status != STATUS_DIVIDED) {
        return -1;
    }

    int left_idx = (idx * 2) + 1;
    int right_idx = left_idx + 1;
    if (right_idx >= TOTAL_NODES) {
        return -1;
    }

    uint32_t half = (uint32_t)(size >> 1);
    if (target_offset < node_offset + half) {
        return locate_node_for_offset(left_idx, target_offset);
    }
    return locate_node_for_offset(right_idx, target_offset);
}


void initMemory(void) {
    build_tree(0, MAX_BLOCK_ORDER, 0);
    allocator_initialized = 1;
    total_free_bytes = TOTAL_HEAP_SIZE;
}

void * myMalloc(int size) {
    if (!allocator_initialized) {
        initMemory();
    }

    if (size <= 0 || size > TOTAL_HEAP_SIZE) {
        return NULL;
    }

    int order = calculate_order(size);

    if (order > MAX_BLOCK_ORDER) {
        return NULL;
    }

    int allocated_idx = find_and_allocate(0, order);
    if (allocated_idx < 0) {
        return NULL;
    }

    TreeNode *allocated_node = &tree_nodes[allocated_idx];
    int block_size = get_block_size(allocated_node->block_order);
    total_free_bytes -= block_size;

    return heap + allocated_node->offset;
}

void myFree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    if (!allocator_initialized) {
        return;
    }

    uint8_t *ptr_byte = (uint8_t *)ptr;
    uint8_t *heap_start = heap;
    uint8_t *heap_end = heap + TOTAL_HEAP_SIZE;

    if (ptr_byte < heap_start || ptr_byte >= heap_end) {
        return;
    }

    uint32_t offset = (uint32_t)(ptr_byte - heap_start);
    int node_index = locate_node_for_offset(0, offset);
    if (node_index < 0 || node_index >= TOTAL_NODES) {
        return;
    }

    TreeNode *node_to_free = &tree_nodes[node_index];

    if (node_to_free->status != STATUS_OCCUPIED) {
        return;
    }

    node_to_free->status = STATUS_AVAILABLE;
    total_free_bytes += get_block_size(node_to_free->block_order);
    merge_upwards(node_index);
}

void memstats(int *total, int *used, int *available) {
    if (!allocator_initialized) {
        initMemory();
    }
    if (total != NULL) {
        *total = TOTAL_HEAP_SIZE;
    }

    if (available != NULL) {
        *available = total_free_bytes;
    }
    if (used != NULL) {
        *used = TOTAL_HEAP_SIZE - total_free_bytes;
    }
}

int isValidHeapPtr(void *ptr) {
    if (ptr == NULL) {
        return 0;
    }
    
    uint8_t *ptr_byte = (uint8_t *)ptr;
    uint8_t *heap_start = heap;
    uint8_t *heap_end = heap + TOTAL_HEAP_SIZE;
    
    // Check if pointer is within heap bounds
    if (ptr_byte < heap_start || ptr_byte >= heap_end) {
        return 0;
    }
    
    uint32_t offset = (uint32_t)(ptr_byte - heap_start);
    int node_index = locate_node_for_offset(0, offset);
    if (node_index < 0 || node_index >= TOTAL_NODES) {
        return 0;
    }
    TreeNode *node = &tree_nodes[node_index];
    
    // Verify the node is actually occupied
    if (node->status != STATUS_OCCUPIED) {
        return 0;
    }
    
    // Verify the pointer matches what malloc would have returned for this node
    if ((heap + node->offset) != ptr_byte) {
        return 0;
    }
    
    return 1;
}
