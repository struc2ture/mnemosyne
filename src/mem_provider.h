#pragma once

#include <cstddef>

void *reserve_mem(size_t size);
void free_mem(void *ptr, size_t size);
