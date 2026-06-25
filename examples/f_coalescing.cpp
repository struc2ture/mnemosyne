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

    std::println("Fresh arena:");
    print_arena_header(1);

    // Only use this so the templated symbols can be used in the debugger
    uint8_t *arena_base = mne_internals::get_arena_base<State>(); (void)arena_base;
    mne_internals::ArenaHeader *arena_header = mne_internals::get_arena_header<State>(); (void)arena_header;
    mne_internals::BlockHeader *first_block = mne_internals::alloc_get_first_block<State>(); (void)first_block;
    // ---

    RPtr<State, int> numbers_1 = mne_alloc<State, int>(3);
    RPtr<State, int> numbers_2 = mne_alloc<State, int>(16);
    RPtr<State, int> numbers_3 = mne_alloc<State, int>(32);

    std::println("Initial state: 3 allocations: 3 ints, 16 ints, 32 ints");
    print_arena_header(1);
    print_alloc_blocks(1);

    mne_free(numbers_2);
    numbers_2 = mne_alloc<State, int>(8);

    std::println("Reallocated numbers_2 to 8 ints. Should split the second block");
    print_arena_header(1);
    print_alloc_blocks(1);

    mne_free(numbers_3);
    std::println("Freed numbers_3. Should coalesce the fourth block, with its previous, third, block");
    print_arena_header(1);
    print_alloc_blocks(1);

    mne_free(numbers_1);
    std::println("Freed numbers_1. Should not coalesce");
    print_arena_header(1);
    print_alloc_blocks(1);

    mne_free(numbers_2);
    std::println("Freed numbers_2. Should coalesce with its next, third, block and its previous, first block. Result - One whole free block");
    print_arena_header(1);
    print_alloc_blocks(1);

    return 0;
}
