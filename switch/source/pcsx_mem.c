#include "pcsx_mem.h"

void *pl_mmap(unsigned int size) {
    return psxMap(0, size, 0, MAP_TAG_VRAM);
}

void pl_munmap(void *ptr, unsigned int size) {
    psxUnmap(ptr, size, MAP_TAG_VRAM);
}

void *psxMap(unsigned long addr, size_t size, int is_fixed, enum psxMapTag tag) {
    return malloc(size);
}

void psxUnmap(void *ptr, size_t size, enum psxMapTag tag) {
    free(ptr);
}
