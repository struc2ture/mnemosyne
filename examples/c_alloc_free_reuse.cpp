// #include <print>

#include "mem.h"
#include "mem_provider.h"
#include "util.h"

struct State
{
    RPtr<State, int> numbers;
};

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

    state.numbers = arena_alloc<State, int>(10);

    for (int i = 0; i < 10; i++)
    {
        state.numbers[i] = i + 1;
    }

    arena_free(state.numbers);

    state.numbers = arena_alloc<State, int>(8);

    arena_free(state.numbers);

    state.numbers = arena_alloc<State, int>(20);

    arena_free(state.numbers);

    return 0;
}
