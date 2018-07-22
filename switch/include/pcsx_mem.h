#include <stdio.h>
#include <switch.h>
#include "psxmem_map.h"

extern void *pl_mmap(unsigned int size);
extern void pl_munmap(void *ptr, unsigned int size);
