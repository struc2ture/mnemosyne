#pragma once

#include <cstddef>
#include <cstring>
#include <print>

#include "mnemosyne.h"

#define KB(value) (  (value) * 1024LL)
#define MB(value) (KB(value) * 1024LL)
#define GB(value) (MB(value) * 1024LL)

void *reserve_mem(size_t size);
void free_mem(void *ptr, size_t size);

inline void indent(int indent_level)
{
    constexpr int space_count = 2;
    for (int i = 0; i < indent_level * space_count; i++) std::print(" ");
}

struct State;

inline void print_arena_header(int indent_level = 0)
{
    mne_internals::ArenaHeader *arena_header = mne_internals::get_arena_header<State>();

    indent(indent_level);     std::println("arena_header:");
    indent(indent_level + 1); std::println("arena_base = {}", (void *)mne_internals::get_arena_base<State>());
    indent(indent_level + 1); std::println("cap = {}", arena_header->cap);
    indent(indent_level + 1); std::println("used = {}", arena_header->used);
    indent(indent_level + 1); std::println("first_block_offset = {}", arena_header->first_block_offset);
    indent(indent_level + 1); std::println("free_block_offset = {}", arena_header->free_block_offset);
}

inline std::string get_magic_string(mne_internals::BlockHeader *block)
{
    std::string magic_string;
    switch (block->magic)
    {
        case 0x12345678:
            magic_string = "FRESH";
            break;
        case 0x44444444:
            magic_string = "SPLIT";
            break;
        case 0x55555555:
            magic_string = "FREE";
            break;
        case 0x66666666:
            magic_string = "COALESCED";
            break;
        case 0x77777777:
            magic_string = "REUSED";
            break;
        default:
            magic_string = "UNKNOWN";
            break;
    }
    return magic_string;
}

inline size_t get_usable_offset(mne_internals::BlockHeader *block)
{
    uint8_t *usable_ptr = (uint8_t *)mne_internals::alloc_get_usable_ptr(block);
    uint8_t *arena_base = mne_internals::get_arena_base<State>();
    size_t usable_offset = usable_ptr - arena_base;
    return usable_offset;
}

inline void print_alloc_block(mne_internals::BlockHeader *block, int i, int indent_level)
{
    indent(indent_level);     std::println("[{}] block:", i);
    indent(indent_level + 1); std::println("size = {}", block->size);
    indent(indent_level + 1); std::println("prev_size = {}", block->prev_size);
    indent(indent_level + 1); std::println("is_free = {}", block->is_free);
    indent(indent_level + 1); std::println("magic = {}", get_magic_string(block));
    indent(indent_level + 1); std::println("usable offset = {}", get_usable_offset(block));
}

inline void print_alloc_blocks(int indent_level = 0)
{
    indent(indent_level); std::println("blocks:");
    int i = 0;
    mne_internals::BlockHeader *block = mne_internals::alloc_get_first_block<State>();
    while (block)
    {
        print_alloc_block(block, i, indent_level + 1);
        block = mne_internals::alloc_get_next_block<State>(block);
        i++;
    }
    if (i == 0)
    {
        indent(indent_level + 1); std::println("none");
    }
}
