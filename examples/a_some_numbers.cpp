#include <iostream>

#include "_ex_common.h"

#include "mnemosyne.h"

struct State
{
    RPtr<State, int> some_numbers;
};

constexpr int number_count = 10;

void print_numbers(State &state)
{
    std::cout << "Numbers: ";
    for (int i = 0; i < number_count; i++)
    {
        std::cout << state.some_numbers[i] << ", ";
    }
    std::cout << "\n";
}

void task(void *mem)
{
    bool is_initialized;
    State &state = arena_attach_owning<State>(mem, is_initialized); (void)state;

    if (!is_initialized)
    {
        state.some_numbers = arena_alloc<State, int>(number_count);

        for (int i = 0; i < number_count; i++)
        {
            state.some_numbers[i] = i + 1;
        }

        std::println("TASK: Initialized numbers.");
        print_numbers(state);
    }
    else
    {
        for (int i = 0; i < number_count; i++)
        {
            state.some_numbers[i] *= 10;
        }

        std::println("TASK: Modified numbers.");
        print_numbers(state);
    }
}

int main()
{
    size_t mem_size = MB(4);
    void *mem = reserve_mem(mem_size);
    std::println("Reserved {} bytes at {}", mem_size, mem);

    std::println("Running task with mem at {}", mem);
    task(mem);

    arena_detach<State>();
    void *mem_two = reserve_mem(mem_size);
    memcpy(mem_two, mem, mem_size);
    free_mem(mem, mem_size);
    std::println("Copied memory block from {} to {}", mem, mem_two);

    std::println("Running task with mem at {}", mem_two);
    task(mem_two);

    free_mem(mem_two, mem_size);

    return 0;
}
