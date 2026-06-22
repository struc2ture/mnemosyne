#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>

void *reserve_mem(size_t size)
{
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    *((size_t *)mem) = size;
    return mem;
}

void free_mem(void *ptr, size_t size)
{
    munmap(ptr, size);
}