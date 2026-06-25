#include <print>

#include "mem.h"
#include "mem_provider.h"
#include "util.h"

struct State
{
    RPtr<State, int> numbers;
};

void indent(int indent_level)
{
    constexpr int space_count = 2;
    for (int i = 0; i < indent_level * space_count; i++) std::print(" ");
}

void print_arena_header(int indent_level = 0)
{
    ArenaHeader *arena_header = arena_get_header<State>();

    indent(indent_level);     std::println("arena_header:");
    indent(indent_level + 1); std::println("cap = {}", arena_header->cap);
    indent(indent_level + 1); std::println("used = {}", arena_header->used);
    indent(indent_level + 1); std::println("first_block_offset = {}", arena_header->first_block_offset);
    indent(indent_level + 1); std::println("free_block_offset = {}", arena_header->free_block_offset);
}

std::string get_magic_string(BlockHeader *block)
{
    std::string magic_string;
    switch (block->magic)
    {
        case 0x12345678:
            magic_string = "FRESH";
            break;
        case 0x55555555:
            magic_string = "FREE";
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

size_t get_usable_offset(BlockHeader *block)
{
    uint8_t *usable_ptr = (uint8_t *)_arena_alloc_get_usable_ptr(block);
    uint8_t *arena_base = arena_get_base<State>();
    size_t usable_offset = usable_ptr - arena_base;
    return usable_offset;
}

void print_alloc_block(BlockHeader *block, int i, int indent_level)
{
    indent(indent_level);     std::println("[{}] block:", i);
    indent(indent_level + 1); std::println("size = {}", block->size);
    indent(indent_level + 1); std::println("prev = {}", block->prev);
    indent(indent_level + 1); std::println("is_free = {}", block->is_free);
    indent(indent_level + 1); std::println("magic = {}", get_magic_string(block));
    indent(indent_level + 1); std::println("usable offset = {}", get_usable_offset(block));
}

void print_alloc_blocks(int indent_level = 0)
{
    indent(indent_level); std::println("blocks:");
    int i = 0;
    BlockHeader *block = _arena_alloc_get_first_block<State>();
    while (block)
    {
        print_alloc_block(block, i, indent_level + 1);
        block = _arena_alloc_get_next_block<State>(block);
        i++;
    }
    if (i == 0)
    {
        indent(indent_level + 1); std::println("none");
    }
}

void print_numbers_rel_ptr(State &state, int indent_level = 0)
{
    indent(indent_level); std::println("state.numbers.offset = {}", state.numbers.offset);
}

int main()
{
    size_t mem_size = MB(4);
    void *mem = reserve_mem(mem_size);

    bool is_initialized;
    State &state = arena_attach_owning<State>(mem, is_initialized); (void)state;

    // Only use this so the templated symbols can be used in the debugger
    uint8_t *arena_base = arena_get_base<State>(); (void)arena_base;
    ArenaHeader *arena_header = arena_get_header<State>(); (void)arena_header;
    BlockHeader *first_block = _arena_alloc_get_first_block<State>(); (void)first_block;
    // ---

    std::println("Fresh arena");
    print_arena_header(1);
    print_alloc_blocks(1);

    state.numbers = arena_alloc<State, int>(10);

    std::println("Allocated 10 ints");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(state, 1);

    for (int i = 0; i < 10; i++)
    {
        state.numbers[i] = i + 1;
    }

    arena_free(state.numbers);

    std::println("Freed previously allocated");
    print_arena_header(1);
    print_alloc_blocks(1);

    state.numbers = arena_alloc<State, int>(8);

    std::println("Allocated 8 ints. Should fit in the existing block");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(state, 1);

    arena_free(state.numbers);

    std::println("Freed previously allocated");
    print_arena_header(1);
    print_alloc_blocks(1);

    state.numbers = arena_alloc<State, int>(20);

    std::println("Allocated 20 ints. Shouldn't fit in the existing block. Allocate second block.");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(state, 1);

    arena_free(state.numbers);

    std::println("Freed previously allocated");
    print_arena_header(1);
    print_alloc_blocks(1);

    return 0;
}
