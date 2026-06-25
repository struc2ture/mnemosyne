#define DISABLE_COALESCING

#include "_ex_common.h"

#include "mnemosyne.h"

struct State
{
    RPtr<State, int> numbers;
};

void print_numbers_rel_ptr(RPtr<State, int> numbers, int indent_level = 0)
{
    indent(indent_level); std::println("numbers.offset = {}", numbers.offset);
}

int main()
{
    size_t mem_size = MB(4);
    void *mem = reserve_mem(mem_size);

    bool is_initialized;
    State &state = mne_attach_mem_owning<State>(mem, is_initialized); (void)state;

    // Only use this so the templated symbols can be used in the debugger
    uint8_t *arena_base = mne_internals::get_arena_base<State>(); (void)arena_base;
    mne_internals::ArenaHeader *arena_header = mne_internals::get_arena_header<State>(); (void)arena_header;
    mne_internals::BlockHeader *first_block = mne_internals::alloc_get_first_block<State>(); (void)first_block;
    // ---

    state.numbers = mne_alloc<State, int>(3);
    mne_free(state.numbers);
    state.numbers = mne_alloc<State, int>(16);
    mne_free(state.numbers);
    state.numbers = mne_alloc<State, int>(32);
    mne_free(state.numbers);

    std::println("Initial state: 3 allocations, immediately freed: 3 ints, 16 ints, 32 ints");
    print_arena_header(1);
    print_alloc_blocks(1);

    RPtr<State, int> numbers_2  = mne_alloc<State, int>(8);
    std::println("New allocation: for 8 ints. Should reuse the second block and split it.");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(numbers_2, 1);

    RPtr<State, int> numbers_3 = mne_alloc<State, int>(2);
    std::println("New allocation: for 2 ints. Should reuse the first block and not split it, because it's too small.");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(numbers_3, 1);

    RPtr<State, int> numbers_4 = mne_alloc<State, int>();
    std::println("New allocation: for 1 int. Should reuse the third block, which was a split block, and not split it, because it's too small.");
    print_arena_header(1);
    print_alloc_blocks(1);
    print_numbers_rel_ptr(numbers_4, 1);

    mne_free(numbers_2);
    mne_free(numbers_3);
    mne_free(numbers_4);

    std::println("Everything freed");
    print_arena_header(1);
    print_alloc_blocks(1);

    return 0;
}
