#include "ex_common.h"
#include "mem.h"
#include <cstring>

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

    state.numbers = arena_alloc<State, int>(3);
    arena_free(state.numbers);
    state.numbers = arena_alloc<State, int>(16);
    arena_free(state.numbers);
    state.numbers = arena_alloc<State, int>(32);
    arena_free(state.numbers);

    std::println("Initial state: 3 allocations, immediately freed: 3 ints, 16 ints, 32 ints");
    print_arena_header(1);
    print_alloc_blocks(1);

    RPtr<State, int> numbers_2  = arena_alloc<State, int>(8);
    std::println("New allocation: for 8 ints. Should reuse the second block and split it.");
    print_arena_header(1);
    print_alloc_blocks(1);

    RPtr<State, int> numbers_3 = arena_alloc<State, int>(2);
    std::println("New allocation: for 2 ints. Should reuse the first block and not split it, because it's too small.");
    print_arena_header(1);
    print_alloc_blocks(1);

    RPtr<State, int> numbers_4 = arena_alloc<State, int>();
    std::println("New allocation: for 1 int. Should reuse the third block, which was a split block, and not split it, because it's too small.");
    print_arena_header(1);
    print_alloc_blocks(1);

    arena_free(numbers_2);
    arena_free(numbers_3);
    arena_free(numbers_4);

    std::println("Everything freed");
    print_arena_header(1);
    print_alloc_blocks(1);

    return 0;
}
