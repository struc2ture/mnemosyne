#include <cassert>
#include <cstdlib>
#include <iostream>
// #include <print>
#include <sys/mman.h>

#include "mem.h"
#include "mem_provider.h"
#include "util.h"

struct State
{
    RPtr<State, int> some_numbers;
};

constexpr int number_count = 10;

void init(void *mem)
{
    arena_init<State>(mem);
    RPtr<State, State> state = arena_get<State>();

    state->some_numbers = arena_push<State, int>(number_count);

    for (int i = 0; i < number_count; i++)
    {
        state->some_numbers[i] = i + 1;
    }
}

void run(void *mem)
{
    arena_attach<State>(mem);
    RPtr<State, State> state = arena_get<State>();

    std::cout << "Numbers: ";
    for (int i = 0; i < number_count; i++)
    {
        std::cout << state->some_numbers[i] << ", ";
    }
    std::cout << "\n";
}

int main()
{
    size_t mem_size = MB(4);
    void *mem = reserve_mem(mem_size);

    init(mem);
    arena_detach<State>();

    run(mem);

    free_mem(mem, mem_size);

    return 0;
}
