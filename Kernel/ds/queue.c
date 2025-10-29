#include "queue.h"
#include "malloc.h"  // Tu nuevo wrapper
#include "panic.h"

struct Node {
	struct Node *next;
	uint8_t *data;
};

struct QueueCDT {
	struct Node *head;
	struct Node *tail;
	struct Node *cyclicIter;
	size_t dataSize;
	size_t elemCount;
	QueueElemCmpFn cmp;
};

QueueADT createQueue(QueueElemCmpFn cmp, size_t elemSize) {
	QueueADT queue = (QueueADT)malloc(sizeof(struct QueueCDT));
	if (queue == NULL || elemSize == 0) {
		free(queue);
		return NULL;
	}
	queue->head = NULL;
	queue->tail = NULL;
	queue->cyclicIter = NULL;
	queue->dataSize = elemSize;
	queue->elemCount = 0;
	queue->cmp = cmp;
	return queue;
}

QueueADT enqueue(QueueADT queue, void *data) {
	if (queue == NULL || data == NULL) {
		return NULL;
	}

	struct Node *new = (struct Node *)malloc(sizeof(struct Node));

	if (new == NULL) {
		return NULL;
	}

	new->data = malloc(queue->dataSize);
	if (new->data == NULL) {
		free(new);
		return NULL;
	}

	memcpy(new->data, data, queue->dataSize);
	new->next = NULL;

	if (queue->head == NULL) {
		queue->tail = new;
		queue->head = new;
	} else {
		queue->tail->next = new;
		queue->tail = new;
	}

	queue->elemCount++;
	return queue;
}

void *dequeue(QueueADT queue, void *buffer) {
	if (queue == NULL || queue->head == NULL) {
		return NULL;
	}

	struct Node *temp = queue->head;

	if (queue->cyclicIter != NULL && queue->cyclicIter == queue->head) {
		queue->cyclicIter = queue->head->next;
	}

	queue->head = queue->head->next;

	if (queue->head == NULL) {
		queue->tail = NULL;
		queue->cyclicIter = NULL;
	}

	memcpy(buffer, temp->data, queue->dataSize);
	free(temp->data);
	free(temp);

	queue->elemCount--;
	return buffer;
}

void *queueRemove(QueueADT queue, void *data) {
	if (queue == NULL || queue->head == NULL) {
		return NULL;
	}

	if (queue->cmp == NULL) {
		panic("queueRemove: No comparison function provided");
		return NULL;
	}

	struct Node *current = queue->head;
	struct Node *prev = NULL;

	while (current != NULL) {
		if (queue->cmp(current->data, data) == 0) {
			if (prev == NULL) {
				queue->head = current->next;
			} else {
				prev->next = current->next;
			}

			if (current == queue->tail) {
				queue->tail = prev;
			}

			if (queue->cyclicIter != NULL && queue->cyclicIter == current) {
				queue->cyclicIter = current->next;
			}

			free(current->data);
			free(current);
			queue->elemCount--;
			return data;
		}
		prev = current;
		current = current->next;
	}

	return NULL;
}

void *queuePeek(QueueADT queue, void *buffer) {
	if (queue == NULL || queue->head == NULL) {
		return NULL;
	}
	memcpy(buffer, queue->head->data, queue->dataSize);
	return buffer;
}

void queueFree(QueueADT queue) {
	if (queue == NULL) {
		return;
	}

	struct Node *current = queue->head;
	struct Node *next;

	while (current != NULL) {
		next = current->next;
		free(current->data);
		free(current);
		current = next;
	}

	free(queue);
}

size_t queueSize(QueueADT queue) {
	if (queue == NULL) {
		return 0;
	}
	return queue->elemCount;
}

QueueADT queueBeginCyclicIter(QueueADT queue) {
	if (queue == NULL || queue->head == NULL) {
		return NULL;
	}
	queue->cyclicIter = queue->head;
	return queue;
}

void *queueNextCyclicIter(QueueADT queue, void *buffer) {
	if (queue == NULL || queue->cyclicIter == NULL || buffer == NULL) {
		return NULL;
	}

	memcpy(buffer, queue->cyclicIter->data, queue->dataSize);

	if (queue->cyclicIter->next == NULL) {
		queue->cyclicIter = queue->head;
	} else {
		queue->cyclicIter = queue->cyclicIter->next;
	}

	return buffer;
}

int queueCyclicIterLooped(QueueADT queue) {
	if (queue == NULL || queue->cyclicIter == NULL) {
		return 0;
	}
	return queue->cyclicIter == queue->head;
}

int queueIsEmpty(QueueADT queue) { return queueSize(queue) == 0; }

int queueIteratorIsInitialized(QueueADT queue) {
	if (queue == NULL) {
		return 0;
	}
	return queue->cyclicIter != NULL;
}

int queueElementExists(QueueADT queue, void *data) {
	int size = queueSize(queue);
	void *buffer = malloc(queue->dataSize);
	
	if (buffer == NULL) {
		return 0;
	}
	
	int found = 0;

	queueBeginCyclicIter(queue);
	for (int i = 0; i < size && !found; i++) {
		queueNextCyclicIter(queue, buffer);
		if (queue->cmp(buffer, data) == 0) {
			found = 1;
		}
	}
	free(buffer);
	return found;
}