#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

struct ArenaHeader
{
    size_t cap;
    size_t used;
};

// TODO: Should this be uint8_t?
template<typename ArenaT>
void *_arena_base = nullptr;

template<typename ArenaT>
ArenaHeader *_arena_header = nullptr;

template<typename ArenaT, typename ValueT>
struct RPtr
{
    size_t offset = 0;

    ValueT *get_ptr()
    {
        assert(offset > 0 && "Dereferencing null offset relative pointer.");
        uint8_t *base = (uint8_t *)_arena_base<ArenaT>;
        return reinterpret_cast<ValueT *>(base + offset);
    }

    const ValueT *get_ptr() const { return reinterpret_cast<const ValueT *>(get_ptr()); }
    ValueT &operator*() { return get_ptr(); }
    const ValueT &operator*() const { return get_ptr(); }
    ValueT *operator->() { return get_ptr(); }
    const ValueT *operator->() const { return get_ptr(); }
    ValueT &operator[](size_t i) { return get_ptr()[i]; }
    const ValueT &operator[](size_t i) const { return get_ptr()[i]; }
    explicit operator bool() { return offset > 0; }
};

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_push(size_t count = 1);

template<typename ArenaT>
void arena_init(void *mem)
{
    _arena_base<ArenaT> = mem;
    _arena_header<ArenaT> = new (mem) ArenaHeader;

    assert(sizeof(ArenaHeader) + sizeof(ArenaT) <= _arena_header<ArenaT>->cap);

    _arena_header<ArenaT>->used = sizeof(ArenaHeader);

    // push the root struct of the arena on to the arena
    arena_push<ArenaT, ArenaT>();
}

template<typename ArenaT>
void arena_attach(void *mem)
{
    _arena_base<ArenaT> = mem;
    _arena_header<ArenaT> = reinterpret_cast<ArenaHeader *>(mem);
}

template<typename ArenaT>
void arena_detach()
{
    _arena_base<ArenaT> = nullptr;
    _arena_header<ArenaT> = nullptr;
}

inline size_t _arena_get_aligned_pos(size_t pos, size_t align)
{
    size_t aligned = (pos + align - 1) & ~(align - 1);
    return aligned;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_push(size_t count)
{
    size_t start = _arena_get_aligned_pos(_arena_header<ArenaT>->used, alignof(ValueT));
    size_t end = start + count * sizeof(ValueT);
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    RPtr<ArenaT, ValueT> ptr{ start };
    return ptr;
}

template<typename ArenaT>
void *arena_push_size(size_t size, size_t align = 1)
{
    size_t start = _arena_get_aligned_pos(_arena_header<ArenaT>->used, align);
    size_t end = start + size;
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    return _arena_base<ArenaT> + start;
}

template<typename ArenaT>
RPtr<ArenaT, ArenaT> arena_get_root_struct()
{
    RPtr<ArenaT, ArenaT> ptr{ sizeof(ArenaHeader) };
    return ptr;
}

// ---

struct BlockHeader
{
    size_t size;
    BlockHeader *next;
    bool is_free;
    int magic;
};

template<typename ArenaT>
BlockHeader *first_block = nullptr;

template<typename ArenaT, typename ValueT>
BlockHeader *find_free_block(BlockHeader **last)
{
    size_t size = sizeof(ValueT);

    BlockHeader *current = first_block<ArenaT>;
    while (current && !(current->is_free && current->size >= size))
    {
        *last = current;
        current = current->next;
    }
    return current;
}

template<typename ArenaT, typename ValueT>
BlockHeader *request_space(BlockHeader *last)
{
    size_t size = sizeof(ValueT);

    void *ptr = arena_push_size<ArenaT>(sizeof(BlockHeader) + size, alignof(ValueT));

    BlockHeader *block = new (ptr) BlockHeader;
    block->size = size;
    block->next = nullptr;
    block->is_free = false;
    block->magic = 0x12345678;

    if (last)
    {
        last->next = block;
    }

    return block;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_alloc()
{
    BlockHeader *block;

    if (!first_block<ArenaT>)
    {
        block = request_space<ValueT>(nullptr);
        first_block<ArenaT> = block;
    }
    else
    {
        BlockHeader *last = first_block<ArenaT>;
        block = find_free_block<ArenaT, ValueT>(&last);
        if (!block)
        {
            block = request_space<ArenaT, ValueT>(last);
        }
        else
        {
            block->is_free = false;
            block->magic = 0x77777777;
        }
    }

    size_t relative_offset = (uint8_t *)block - (uint8_t *)_arena_base<ArenaT> + sizeof(BlockHeader);
    RPtr<ArenaT, ValueT> r_ptr{ relative_offset };
    return r_ptr;
}

inline BlockHeader *get_block_ptr(void *ptr)
{
    return reinterpret_cast<BlockHeader *>((uint8_t *)ptr - sizeof(BlockHeader));
}

template<typename ArenaT, typename ValueT>
void arena_free(RPtr<ArenaT, ValueT> r_ptr)
{
    if (!r_ptr) return;

    void *ptr = r_ptr.get_ptr();
    BlockHeader *block = get_block_ptr(ptr);
    assert(!block->is_free);
    assert(block->magic = 0x77777777 || block->magic == 0x12345678);
    block->is_free = true;
    block->magic = 0x55555555;
}
