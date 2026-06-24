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
static_assert(sizeof(ArenaHeader) == 16);

template<typename ArenaT>
uint8_t *_arena_base = nullptr;

template<typename ArenaT>
ArenaHeader *_arena_header = nullptr;

template<typename ArenaT, typename ValueT>
struct RPtr
{
    size_t offset = 0;

    ValueT *get_ptr()
    {
        assert(offset > 0 && "Dereferencing null offset relative pointer.");
        return reinterpret_cast<ValueT *>(_arena_base<ArenaT> + offset);
    }

    const ValueT *get_ptr() const { return get_ptr(); }
    ValueT &operator*() { return get_ptr(); }
    const ValueT &operator*() const { return get_ptr(); }
    ValueT *operator->() { return get_ptr(); }
    const ValueT *operator->() const { return get_ptr(); }
    ValueT &operator[](size_t i) { return get_ptr()[i]; }
    const ValueT &operator[](size_t i) const { return get_ptr()[i]; }
    explicit operator bool() { return offset > 0; }
};

template<typename ArenaT>
void *arena_push_size(size_t size, size_t align = 1);

template<typename ArenaT>
ArenaT &arena_get_root_struct();

template<typename ArenaT>
ArenaT &arena_attach_owning(void *mem, bool &is_initialized)
{
    _arena_base<ArenaT> = reinterpret_cast<uint8_t *>(mem);
    _arena_header<ArenaT> = reinterpret_cast<ArenaHeader *>(mem);

    if (_arena_header<ArenaT>->used == 0)
    {
        // Not already initialized. Initialize the arena and the root struct
        _arena_header<ArenaT>->used = sizeof(ArenaHeader);
        // Push root struct
        arena_push_size<ArenaT>(sizeof(ArenaT), alignof(ArenaT));
        is_initialized = false;
    }
    else
    {
        is_initialized = true;
    }

    return arena_get_root_struct<ArenaT>();
}

template<typename ArenaT>
bool arena_attach_non_owning(void *mem)
{
    bool attached;

    _arena_base<ArenaT> = reinterpret_cast<uint8_t *>(mem);
    _arena_header<ArenaT> = reinterpret_cast<ArenaHeader *>(mem);

    if (_arena_header<ArenaT>->used == 0)
    {
        // Not already initialized. Cannot attach memory
        _arena_base<ArenaT> = nullptr;
        _arena_header<ArenaT> = nullptr;
        attached = false;
    }
    else
    {
        attached = true;
    }

    return attached;
}

template<typename ArenaT>
void arena_detach()
{
    _arena_base<ArenaT> = nullptr;
    _arena_header<ArenaT> = nullptr;
}

constexpr size_t _align_up(size_t pos, size_t align)
{
    size_t aligned = (pos + align - 1) & ~(align - 1);
    return aligned;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> _arena_push(size_t count)
{
    size_t start = _align_up(_arena_header<ArenaT>->used, alignof(ValueT));
    size_t end = start + count * sizeof(ValueT);
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    RPtr<ArenaT, ValueT> ptr{ start };
    return ptr;
}

template<typename ArenaT>
void *arena_push_size(size_t size, size_t align)
{
    size_t start = _align_up(_arena_header<ArenaT>->used, align);
    size_t end = start + size;
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    return _arena_base<ArenaT> + start;
}

template<typename ArenaT>
ArenaT &arena_get_root_struct()
{
    ArenaT *root_ptr = reinterpret_cast<ArenaT *>(_arena_base<ArenaT> + sizeof(ArenaHeader));
    return *root_ptr;
}

// ---

struct BlockHeader
{
    size_t size;
    BlockHeader *next;
    bool is_free;
    int magic;
};
static_assert(sizeof(BlockHeader) % alignof(std::max_align_t) == 0);

template<typename ArenaT>
BlockHeader *first_block = nullptr;

template<typename ArenaT>
BlockHeader *find_free_block(BlockHeader **last, size_t size)
{
    BlockHeader *current = first_block<ArenaT>;
    while (current && !(current->is_free && current->size >= size))
    {
        *last = current;
        current = current->next;
    }
    return current;
}

template<typename ArenaT>
BlockHeader *request_space(BlockHeader *last, size_t size)
{
    void *ptr = arena_push_size<ArenaT>(sizeof(BlockHeader) + size, alignof(std::max_align_t));

    BlockHeader *block = reinterpret_cast<BlockHeader *>(ptr);
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
RPtr<ArenaT, ValueT> arena_alloc(size_t count = 1)
{
    BlockHeader *block;

    size_t size = count * sizeof(ValueT);

    if (!first_block<ArenaT>)
    {
        block = request_space<ArenaT>(nullptr, size);
        first_block<ArenaT> = block;
    }
    else
    {
        BlockHeader *last = first_block<ArenaT>;
        block = find_free_block<ArenaT>(&last, size);
        if (!block)
        {
            block = request_space<ArenaT>(last, size);
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

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_alloc_aligned(size_t count = 1)
{
    (void)count;
    // TODO Implement alloc that can be aligned for types that need > max_align_t alignment
    assert(false && "Aligned alloc is not implemented");
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
