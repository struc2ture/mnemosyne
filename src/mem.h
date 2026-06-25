// TODO: Splitting blocks
// TODO: Coalescing blocks
// TODO: There's a problem now that even though the start of each block is aligned,
//       its size doesn't represent the size up to the next block,
//       but the size that the allocation itself needs.
//       On each allocation, the size of the block, should be made a multiple of max_align_t.

// Maybe TODO:
// A separate free list
// realloc and calloc

// Further improvements:
// alligned_alloc for alignments > max_align_t (for SIMD, etc.); keeping track of alignment on block reuse
//      Or: making the regular alloc always alignment-aware based on templated type
// Allocation hierarchy by size (small, large) with different ways of block bookkeeping for different sizes
//      E.g. for small allocations, round up to a size-class -> no O(n) list traversal;
//      for large allocations, pages, etc.
// Reserve and commit memory by pages. Release memory back to the OS.
// Thread-safety for alloc/free operations from different threads using the same arena
// Accounting for strict aliasing rules, using placement new and std::launder, etc.
// Safe containers w/ templates and bound-checking: fixed_array, vector - so it can be built without -fno-strict-aliasing
// Safe counted strings on arena
// (Some) Address Sanitzation: Some slow way of checking any memory access into an arena at runtime. Compile out with a flag.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

struct ArenaHeader
{
    size_t cap;
    size_t used;
    size_t first_block_offset;
    size_t free_block_offset;
};
static_assert(sizeof(ArenaHeader) % alignof(max_align_t) == 0);

template<typename ArenaT> uint8_t *_arena_base = nullptr;
template<typename ArenaT> uint8_t *arena_get_base() { return _arena_base<ArenaT>; }

template<typename ArenaT> ArenaHeader *_arena_header = nullptr;
template<typename ArenaT> ArenaHeader *arena_get_header() { return _arena_header<ArenaT>; }

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

constexpr size_t _arena_align_up(size_t pos, size_t align)
{
    size_t aligned = (pos + align - 1) & ~(align - 1);
    return aligned;
}

template <typename ArenaT>
size_t _arena_alloc_expected_first_block_offset()
{
    size_t arena_header_end = sizeof(ArenaHeader);
    size_t root_struct_start = _arena_align_up(arena_header_end, alignof(max_align_t));
    size_t root_struct_end = root_struct_start + sizeof(ArenaT);
    size_t first_block_start = _arena_align_up(root_struct_end, alignof(max_align_t));
    return first_block_start;
}

template<typename ArenaT>
ArenaT &arena_attach_owning(void *mem, bool &is_initialized)
{
    _arena_base<ArenaT> = reinterpret_cast<uint8_t *>(mem);
    _arena_header<ArenaT> = reinterpret_cast<ArenaHeader *>(mem);

    if (_arena_header<ArenaT>->used == 0)
    {
        // Not already initialized. Initialize the arena and the root struct
        _arena_header<ArenaT>->used = sizeof(ArenaHeader);
        arena_push_size<ArenaT>(sizeof(ArenaT), alignof(max_align_t)); // Push root struct
        _arena_header<ArenaT>->first_block_offset = _arena_alloc_expected_first_block_offset<ArenaT>();
        is_initialized = false;
    }
    else
    {
        assert(_arena_header<ArenaT>->first_block_offset == _arena_alloc_expected_first_block_offset<ArenaT>() && "Root Struct size changed");
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
        assert(_arena_header<ArenaT>->first_block_offset == _arena_alloc_expected_first_block_offset<ArenaT>() && "Root Struct size changed");
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

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> _arena_push(size_t count)
{
    size_t start = _arena_align_up(_arena_header<ArenaT>->used, alignof(ValueT));
    size_t end = start + count * sizeof(ValueT);
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    RPtr<ArenaT, ValueT> ptr{ start };
    return ptr;
}

template<typename ArenaT>
void *arena_push_size(size_t size, size_t align)
{
    size_t start = _arena_align_up(_arena_header<ArenaT>->used, align);
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
    size_t prev;
    bool is_free;
    int magic; // 0x12345678 - freshly allocated block; 0x44444444 - split block; 0x55555555 - freed block; 0x77777777 - freed and reused block
};
static_assert(sizeof(BlockHeader) % alignof(max_align_t) == 0);

template<typename ArenaT> BlockHeader *_arena_alloc_get_first_block()
{
    BlockHeader *block = nullptr;
    if (_arena_header<ArenaT>->first_block_offset < _arena_header<ArenaT>->used)
    {
        block = reinterpret_cast<BlockHeader *>(_arena_base<ArenaT> + _arena_header<ArenaT>->first_block_offset);
    }
    return block;
};

template<typename ArenaT> BlockHeader *_arena_alloc_get_next_block(BlockHeader *curr)
{
    BlockHeader *next = nullptr;
    size_t curr_offset = (uint8_t *)curr - _arena_base<ArenaT>;
    size_t next_offset = curr_offset + sizeof(BlockHeader) + curr->size;
    if (next_offset < _arena_header<ArenaT>->used)
    {
        next = reinterpret_cast<BlockHeader *>(_arena_base<ArenaT> + next_offset);
    }
    return next;
};

inline bool _arena_alloc_block_is_free(BlockHeader *block, size_t size)
{
    return block->is_free && block->size >= size;
}

template<typename ArenaT>
BlockHeader *_arena_alloc_find_free_block(size_t size)
{
    BlockHeader *current = _arena_alloc_get_first_block<ArenaT>();
    while (current && !_arena_alloc_block_is_free(current, size))
    {
        current = _arena_alloc_get_next_block<ArenaT>(current);
    }
    return current;
}

template<typename ArenaT>
BlockHeader *_arena_alloc_request_space(size_t size)
{
    void *ptr = arena_push_size<ArenaT>(sizeof(BlockHeader) + size, alignof(std::max_align_t));

    BlockHeader *block = reinterpret_cast<BlockHeader *>(ptr);
    block->size = _arena_align_up(size, alignof(max_align_t)); // TODO: test this with allocations, the size of which are not a multiple of max align.
    block->is_free = false;
    block->magic = 0x12345678;

    return block;
}

inline BlockHeader *_arena_alloc_get_block_ptr(void *ptr)
{
    return reinterpret_cast<BlockHeader *>((uint8_t *)ptr - sizeof(BlockHeader));
}

inline void *_arena_alloc_get_usable_ptr(BlockHeader *block_ptr)
{
    return reinterpret_cast<void *>((uint8_t *)block_ptr + sizeof(BlockHeader));
}

inline void _arena_alloc_reuse_block(BlockHeader *block, size_t new_size)
{
    constexpr size_t split_block_min_size = sizeof(BlockHeader) + alignof(max_align_t);
    new_size = _arena_align_up(new_size, alignof(max_align_t));
    if (block->size - new_size >= split_block_min_size)
    {
        size_t current_block_start = (size_t)block;
        size_t current_block_end = current_block_start + sizeof(BlockHeader) + block->size;
        size_t split_block_start =  current_block_start + sizeof(BlockHeader) + new_size;

        BlockHeader *split_block = (BlockHeader *)split_block_start;
        split_block->size = current_block_end - split_block_start - sizeof(BlockHeader);
        split_block->is_free = true;
        split_block->magic = 0x44444444;

        block->size = new_size;
    }
    block->is_free = false;
    block->magic = 0x77777777;
}

inline void _arena_alloc_free_block(BlockHeader *block)
{
    block->is_free = true;
    block->magic = 0x55555555;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_alloc(size_t count = 1)
{
    size_t size = count * sizeof(ValueT);

    BlockHeader *block = _arena_alloc_get_first_block<ArenaT>();
    if (!block) block = _arena_alloc_request_space<ArenaT>(size);
    else
    {
        block = _arena_alloc_find_free_block<ArenaT>(size);
        if (!block) block = _arena_alloc_request_space<ArenaT>(size);
        else _arena_alloc_reuse_block(block, size);
    }

    size_t relative_offset = (uint8_t *)_arena_alloc_get_usable_ptr(block) - _arena_base<ArenaT>;
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

template<typename ArenaT, typename ValueT>
void arena_free(RPtr<ArenaT, ValueT> r_ptr)
{
    if (!r_ptr) return;

    void *ptr = r_ptr.get_ptr();
    BlockHeader *block = _arena_alloc_get_block_ptr(ptr);
    assert(!block->is_free);
    assert(block->magic = 0x77777777 || block->magic == 0x12345678);
    _arena_alloc_free_block(block);
}
