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
static size_t get_block_size(int order) {
    return (size_t)1u << order;
}
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


static int calculate_order(size_t size) {
    int order = MIN_BLOCK_ORDER;

    while (order <= MAX_BLOCK_ORDER && get_block_size(order) < size) {
        order++;
    }

    return order;
}

static TreeNode *find_and_allocate(TreeNode *current, int order) {
    if (current == NULL) {
        return NULL;
    }

    if (current->status == STATUS_OCCUPIED) {
        return NULL;
    }

    if (current->block_order < order) {
        return NULL;
    }

    if (current->block_order == order) {
        if (current->status == STATUS_AVAILABLE) {
            current->status = STATUS_OCCUPIED;
            return current;
        }
        return NULL;
    }

    if (current->left_child == NULL || current->right_child == NULL) {
        return NULL;
    }

    if (current->status == STATUS_AVAILABLE) {
        current->status = STATUS_DIVIDED;
    }

    TreeNode *allocated = find_and_allocate(current->left_child, order);
    if (allocated != NULL) {
        return allocated;
    }

    allocated = find_and_allocate(current->right_child, order);
    if (allocated != NULL) {
        return allocated;
    }

    if (current->left_child->status == STATUS_AVAILABLE && current->right_child->status == STATUS_AVAILABLE) {
        current->status = STATUS_AVAILABLE;
    }

    return NULL;
}

static void merge_upwards(TreeNode *current) {
    while (current != NULL) {
        if (current->left_child != NULL && current->right_child != NULL) {
            if (current->left_child->status == STATUS_AVAILABLE && current->right_child->status == STATUS_AVAILABLE) {
                current->status = STATUS_AVAILABLE;
                current = current->parent_node;
            } else {
                current->status = STATUS_DIVIDED;
                current = NULL;
            }
        } else {
            current = current->parent_node;
        }
    }
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

void * malloc(size_t size) {
    if (root_node == NULL) {
        init_mm();
    }

    if (size == 0 || size > TOTAL_HEAP_SIZE) {
        return NULL;
    }

    if (size > TOTAL_HEAP_SIZE - sizeof(AllocMetadata)) {
        return NULL;
    }

    size_t total_needed = size + sizeof(AllocMetadata);
    int order = calculate_order(total_needed);

    if (order > MAX_BLOCK_ORDER) {
        return NULL;
    }

    TreeNode *allocated_node = find_and_allocate(root_node, order);
    if (allocated_node == NULL) {
        return NULL;
    }

    AllocMetadata *metadata = (AllocMetadata *)allocated_node->mem_addr;
    metadata->tree_node = allocated_node;

    return allocated_node->mem_addr + sizeof(AllocMetadata);
}

void free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    if (root_node == NULL) {
        return;
    }

    AllocMetadata *metadata = (AllocMetadata *)((uint8_t *)ptr - sizeof(AllocMetadata));
    TreeNode *node_to_free = metadata->tree_node;

    if (node_to_free == NULL || node_to_free->status != STATUS_OCCUPIED) {
        return;
    }

    metadata->tree_node = NULL;
    node_to_free->status = STATUS_AVAILABLE;
    merge_upwards(node_to_free->parent_node);
}

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

int is_valid_heap_ptr(void *ptr) {
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
    
    // Check if there's enough space for the metadata header
    if (ptr_byte - sizeof(AllocMetadata) < heap_start) {
        return 0;
    }
    
    // Verify this is actually a valid allocated block by checking metadata
    AllocMetadata *metadata = (AllocMetadata *)(ptr_byte - sizeof(AllocMetadata));
    TreeNode *node = metadata->tree_node;
    
    // Verify the tree node pointer is valid (within tree_nodes array)
    if (node < tree_nodes || node >= tree_nodes + TOTAL_NODES) {
        return 0;
    }
    
    // Verify the node is actually occupied
    if (node->status != STATUS_OCCUPIED) {
        return 0;
    }
    
    // Verify the pointer matches what malloc would have returned for this node
    if (node->mem_addr + sizeof(AllocMetadata) != ptr_byte) {
        return 0;
    }
    
    return 1;
}
