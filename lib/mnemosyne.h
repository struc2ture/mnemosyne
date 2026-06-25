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
// (Micro improvement) If last block is free but is not big enough, expand its size.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace mne_internals {

struct ArenaHeader
{
    size_t cap;
    size_t used;
    size_t first_block_offset;
    size_t free_block_offset;
};
static_assert(sizeof(ArenaHeader) % alignof(max_align_t) == 0);

struct BlockHeader
{
    size_t size;
    size_t prev_size;
    bool is_free;
    int magic; // 0x12345678 - freshly allocated block; 0x44444444 - split block; 0x55555555 - freed block; 0x66666666 - coalesced; 0x77777777 - freed and reused block
};
static_assert(sizeof(BlockHeader) % alignof(max_align_t) == 0);

template<typename ArenaT> uint8_t *_arena_base = nullptr;
template<typename ArenaT> uint8_t *get_arena_base() { return _arena_base<ArenaT>; }

template<typename ArenaT> ArenaHeader *_arena_header = nullptr;
template<typename ArenaT> ArenaHeader *get_arena_header() { return _arena_header<ArenaT>; }

constexpr size_t align_up(size_t pos, size_t align)
{
    size_t aligned = (pos + align - 1) & ~(align - 1);
    return aligned;
}

template<typename ArenaT>
void *arena_push_size(size_t size, size_t align)
{
    size_t start = align_up(_arena_header<ArenaT>->used, align);
    size_t end = start + size;
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    return _arena_base<ArenaT> + start;
}

template <typename ArenaT>
size_t alloc_expected_first_block_offset()
{
    size_t arena_header_end = sizeof(ArenaHeader);
    size_t root_struct_start = align_up(arena_header_end, alignof(max_align_t));
    size_t root_struct_end = root_struct_start + sizeof(ArenaT);
    size_t first_block_start = align_up(root_struct_end, alignof(max_align_t));
    return first_block_start;
}

template<typename ArenaT>
BlockHeader *alloc_get_first_block()
{
    BlockHeader *block = nullptr;
    if (_arena_header<ArenaT>->first_block_offset < _arena_header<ArenaT>->used)
    {
        block = (BlockHeader *)(mne_internals::_arena_base<ArenaT> + _arena_header<ArenaT>->first_block_offset);
    }
    return block;
}

template<typename ArenaT>
BlockHeader *alloc_get_next_block(BlockHeader *curr)
{
    BlockHeader *next = nullptr;
    size_t curr_offset = (uint8_t *)curr - mne_internals::_arena_base<ArenaT>;
    size_t next_offset = curr_offset + sizeof(BlockHeader) + curr->size;
    if (next_offset < _arena_header<ArenaT>->used)
    {
        next = (BlockHeader *)(mne_internals::_arena_base<ArenaT> + next_offset);
    }
    return next;
}

inline
BlockHeader *alloc_get_prev_block(BlockHeader *curr)
{
    BlockHeader *prev = nullptr;
    if (curr->prev_size > 0)
    {
        size_t curr_addr = (size_t)curr;
        size_t prev_addr = curr_addr - curr->prev_size - sizeof(BlockHeader);
        prev = (BlockHeader *)(prev_addr);
    }
    return prev;
};

inline
bool alloc_block_is_free(BlockHeader *block, size_t size)
{
    return block->is_free && block->size >= size;
}

template<typename ArenaT>
BlockHeader *alloc_find_free_block(size_t size, BlockHeader **previous)
{
    BlockHeader *current = alloc_get_first_block<ArenaT>();
    while (current && !alloc_block_is_free(current, size))
    {
        *previous = current;
        current = alloc_get_next_block<ArenaT>(current);
    }
    return current;
}

template<typename ArenaT>
BlockHeader *alloc_request_space(size_t size, BlockHeader *previous)
{
    void *ptr = arena_push_size<ArenaT>(sizeof(BlockHeader) + size, alignof(std::max_align_t));

    BlockHeader *block = reinterpret_cast<BlockHeader *>(ptr);
    block->size = align_up(size, alignof(max_align_t));
    block->prev_size = previous ? previous->size : 0;
    block->is_free = false;
    block->magic = 0x12345678;

    return block;
}

inline
BlockHeader *alloc_get_block_ptr(void *ptr)
{
    return reinterpret_cast<BlockHeader *>((uint8_t *)ptr - sizeof(BlockHeader));
}

inline
void *alloc_get_usable_ptr(BlockHeader *block_ptr)
{
    return reinterpret_cast<void *>((uint8_t *)block_ptr + sizeof(BlockHeader));
}

inline
void alloc_reuse_block(BlockHeader *block, size_t new_size)
{
    constexpr size_t split_block_min_size = sizeof(BlockHeader) + alignof(max_align_t);
    new_size = align_up(new_size, alignof(max_align_t));
    if (block->size - new_size >= split_block_min_size)
    {
        size_t current_block_start = (size_t)block;
        size_t current_block_end = current_block_start + sizeof(BlockHeader) + block->size;
        size_t split_block_start =  current_block_start + sizeof(BlockHeader) + new_size;

        block->size = new_size;

        BlockHeader *split_block = (BlockHeader *)split_block_start;
        split_block->size = current_block_end - split_block_start - sizeof(BlockHeader);
        split_block->prev_size = block->size;
        split_block->is_free = true;
        split_block->magic = 0x44444444;

        BlockHeader *next_block = (BlockHeader *)current_block_end;
        next_block->prev_size = split_block->size;
    }
    block->is_free = false;
    block->magic = 0x77777777;
}

template <typename ArenaT>
void alloc_free_block(BlockHeader *block)
{
#ifdef DISABLE_COALESCING
    block->is_free = true;
    block->magic = 0x55555555;
#else
    bool have_coalesced = false;

    // Try coalesce with the next block
    BlockHeader *next_block = alloc_get_next_block<ArenaT>(block);
    if (next_block && next_block->is_free)
    {
        block->is_free = true;
        block->size += sizeof(BlockHeader) + next_block->size;
        block->magic = 0x66666666;

        BlockHeader *new_next_block = alloc_get_next_block<ArenaT>(block);
        if (new_next_block)
        {
            new_next_block->prev_size = block->size;
        }

        have_coalesced = true;
    }

    // Try coalesce with the prev block
    BlockHeader *prev_block = alloc_get_prev_block(block);
    if (prev_block && prev_block->is_free)
    {
        prev_block->size += sizeof(BlockHeader) + block->size;
        prev_block->magic = 0x66666666;

        BlockHeader *new_next_block = alloc_get_next_block<ArenaT>(prev_block);
        if (new_next_block)
        {
            new_next_block->prev_size = prev_block->size;
        }

        have_coalesced = true;
    }

    if (!have_coalesced)
    {
        block->is_free = true;
        block->magic = 0x55555555;
    }
#endif
}
}; // namespace mne_internals

template<typename ArenaT, typename ValueT>
struct RPtr
{
    size_t offset = 0;

    ValueT *get_ptr()
    {
        assert(offset > 0 && "Dereferencing null offset relative pointer.");
        return reinterpret_cast<ValueT *>(mne_internals::_arena_base<ArenaT> + offset);
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
ArenaT &mne_get_root_struct()
{
    ArenaT *root_ptr = (ArenaT *)(mne_internals::_arena_base<ArenaT> + sizeof(mne_internals::ArenaHeader));
    return *root_ptr;
}

template<typename ArenaT>
ArenaT &mne_attach_mem_owning(void *mem, bool &is_initialized)
{
    mne_internals::_arena_base<ArenaT> = (uint8_t *)mem;
    mne_internals::_arena_header<ArenaT> = (mne_internals::ArenaHeader *)mem;

    if (mne_internals::_arena_header<ArenaT>->used == 0)
    {
        // Not already initialized. Initialize the arena and the root struct
        mne_internals::_arena_header<ArenaT>->used = sizeof(mne_internals::ArenaHeader);
        mne_internals::arena_push_size<ArenaT>(sizeof(ArenaT), alignof(max_align_t)); // Push root struct
        mne_internals::_arena_header<ArenaT>->first_block_offset = mne_internals::alloc_expected_first_block_offset<ArenaT>();
        is_initialized = false;
    }
    else
    {
        assert(mne_internals::_arena_header<ArenaT>->first_block_offset == mne_internals::alloc_expected_first_block_offset<ArenaT>() && "Root Struct size changed");
        is_initialized = true;
    }

    return mne_get_root_struct<ArenaT>();
}

template<typename ArenaT>
bool mne_attach_mem_non_owning(void *mem)
{
    bool attached;

    mne_internals::_arena_base<ArenaT> = (uint8_t *)mem;
    mne_internals::_arena_header<ArenaT> = (mne_internals::ArenaHeader *)mem;

    if (mne_internals::_arena_header<ArenaT>->used == 0)
    {
        // Not already initialized. Cannot attach memory
        mne_internals::_arena_base<ArenaT> = nullptr;
        mne_internals::_arena_header<ArenaT> = nullptr;
        attached = false;
    }
    else
    {
        assert(mne_internals::_arena_header<ArenaT>->first_block_offset == mne_internals::alloc_expected_first_block_offset<ArenaT>() && "Root Struct size changed");
        attached = true;
    }

    return attached;
}

template<typename ArenaT>
void mne_detach_mem()
{
    mne_internals::_arena_base<ArenaT> = nullptr;
    mne_internals::_arena_header<ArenaT> = nullptr;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> mne_alloc(size_t count = 1)
{
    size_t size = count * sizeof(ValueT);

    mne_internals::BlockHeader *block = mne_internals::alloc_get_first_block<ArenaT>();
    if (!block) block = mne_internals::alloc_request_space<ArenaT>(size, nullptr);
    else
    {
        mne_internals::BlockHeader *last;
        block = mne_internals::alloc_find_free_block<ArenaT>(size, &last);
        if (!block) block = mne_internals::alloc_request_space<ArenaT>(size, last);
        else mne_internals::alloc_reuse_block(block, size);
    }

    size_t relative_offset = (uint8_t *)mne_internals::alloc_get_usable_ptr(block) - mne_internals::_arena_base<ArenaT>;
    RPtr<ArenaT, ValueT> r_ptr{ relative_offset };
    return r_ptr;
}

template<typename ArenaT, typename ValueT>
void mne_free(RPtr<ArenaT, ValueT> r_ptr)
{
    if (!r_ptr) return;

    void *ptr = r_ptr.get_ptr();
    mne_internals::BlockHeader *block = mne_internals::alloc_get_block_ptr(ptr);
    assert(!block->is_free);
    assert(block->magic = 0x77777777 || block->magic == 0x12345678);
    mne_internals::alloc_free_block<ArenaT>(block);
}
