#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void initQueue(void);
void destroyQueue(void);

void enqueue(void*);
void* dequeue(void);

bool tryDequeue(void**);

size_t size(void);
size_t visited(void); // dont capture a lock

size_t waiting(void);

