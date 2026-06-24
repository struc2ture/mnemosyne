#pragma once

#include <cassert>
#include <cstddef>
#include <new>

struct BlockHeader
{
    size_t size;
    BlockHeader *next;
    bool is_free;
    int magic;
};

inline uint8_t *_arena_base = nullptr;
inline size_t _arena_used = 0;
inline size_t _arena_cap = 0;
inline BlockHeader *first_block = nullptr;

inline BlockHeader *find_free_block(BlockHeader **last, size_t size)
{
    BlockHeader *current = first_block;
    while (current && !(current->is_free && current->size >= size))
    {
        *last = current;
        current = current->next;
    }
    return current;
}

inline BlockHeader *request_space(BlockHeader *last, size_t size)
{
    assert(_arena_used + sizeof(BlockHeader) + size <= _arena_cap);

    uint8_t *ptr = _arena_base + _arena_used;
    _arena_used += sizeof(BlockHeader) + size;

    BlockHeader *block = new (ptr) BlockHeader;
    block->size = size;
    block->next = nullptr;
    block->is_free =false;
    block->magic = 0x12345678;

    if (last)
    {
        last->next = block;
    }

    return block;
}

inline void *malloc(size_t size)
{
    BlockHeader *block;

    if (size <= 0)
    {
        return nullptr;
    }

    if (!first_block)
    {
        block = request_space(nullptr, size);
        if (!block)
        {
            return nullptr;
        }
        first_block = block;
    }
    else
    {
        BlockHeader *last = first_block;
        block = find_free_block(&last, size);
        if (!block)
        {
            block = request_space(last, size);
            if (!block)
            {
                return nullptr;
            }
        }
        else
        {
            block->is_free = false;
            block->magic = 0x77777777;
        }
    }

    void *user_ptr = (uint8_t *)block + sizeof(BlockHeader);
    return user_ptr;
}

inline BlockHeader *get_block_ptr(void *ptr)
{
    return reinterpret_cast<BlockHeader *>((uint8_t *)ptr - sizeof(BlockHeader));
}

inline void free(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    BlockHeader *block = get_block_ptr(ptr);
    assert(block->is_free);
    assert(block->magic == 0x77777777 || block->magic == 0x12345678);
    block->is_free = true;
    block->magic = 0x55555555;
}
