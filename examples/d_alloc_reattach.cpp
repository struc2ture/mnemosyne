#include "_ex_common.h"

#include "mnemosyne.h"

struct State
{
    RPtr<State, int> numbers;
};

void print_numbers_rel_ptr(State &state, int indent_level = 0)
{
    indent(indent_level); std::println("state.numbers.offset = {}", state.numbers.offset);
}

void print_numbers(State &state, int number_count ,int indent_level = 0)
{
    indent(indent_level); std::println("Numbers:");

    indent(indent_level + 1);
    for (int i = 0; i < number_count; i++)
    {
        std::print("{}", state.numbers[i]);
        if (i < number_count - 1) std::print(", ");
        else std::println();
    }

    if (number_count <= 0)
    {
        std::println("none");
    }
}

int main()
{
    size_t mem_size = MB(4);
    void *mem = reserve_mem(mem_size);

    {
        bool is_initialized;
        State &state = mne_attach_mem_owning<State>(mem, is_initialized); (void)state;

        // Only use this so the templated symbols can be used in the debugger
        uint8_t *arena_base = mne_internals::get_arena_base<State>(); (void)arena_base;
        mne_internals::ArenaHeader *arena_header = mne_internals::get_arena_header<State>(); (void)arena_header;
        mne_internals::BlockHeader *first_block = mne_internals::alloc_get_first_block<State>(); (void)first_block;
        // ---


        state.numbers = mne_alloc<State, int>(10);
        mne_free(state.numbers);
        state.numbers = mne_alloc<State, int>(20);

        for (int i = 0; i < 20; i++)
        {
            state.numbers[i] = i + 1;
        }

        std::println("Initial state: allocated for 20 ints, after freeing 10 ints");
        print_arena_header(1);
        print_alloc_blocks(1);
        print_numbers_rel_ptr(state, 1);
        print_numbers(state, 20, 1);
    }

    mne_detach_mem<State>();

    void *new_mem = reserve_mem(mem_size);
    memcpy(new_mem, mem, mem_size);
    free_mem(mem, mem_size);
    std::println("Detached arena. Got a new block of memory at {}. Copied all bytes from {} to {}", new_mem, mem, new_mem);

    {
        bool attached = mne_attach_mem_non_owning<State>(new_mem);
        if (attached)
        {
            State &state =mne_get_root_struct<State>();

            std::println("Attached arena to new_mem");
            print_arena_header(1);
            print_alloc_blocks(1);
            print_numbers_rel_ptr(state, 1);
            print_numbers(state, 20, 1);
        }
    }

    return 0;
}
