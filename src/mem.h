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

template <typename ValueT>
size_t _arena_get_aligned_pos(size_t pos)
{
    size_t align = alignof(ValueT);
    size_t aligned = (pos + align - 1) & ~(align - 1);
    return aligned;
}

template<typename ArenaT, typename ValueT>
RPtr<ArenaT, ValueT> arena_push(size_t count)
{
    size_t start = _arena_get_aligned_pos<ValueT>(_arena_header<ArenaT>->used);
    assert(start < _arena_header<ArenaT>->cap);
    size_t end = start + count * sizeof(ValueT);
    assert (end <=_arena_header<ArenaT>->cap);
    _arena_header<ArenaT>->used = end;
    RPtr<ArenaT, ValueT> ptr{ start };
    return ptr;
}

template<typename ArenaT>
RPtr<ArenaT, ArenaT> arena_get()
{
    RPtr<ArenaT, ArenaT> ptr{ sizeof(ArenaHeader) };
    return ptr;
}
