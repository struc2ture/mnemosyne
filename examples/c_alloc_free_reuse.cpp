#include "ex_common.h"

struct State
{
    RPtr<State, int> numbers;
};

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
