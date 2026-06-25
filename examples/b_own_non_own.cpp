#include "_ex_common.h"

#include "mnemosyne.h"

struct State
{
};

void task_owning(void *mem)
{
    bool is_initialized;
    State &state = mne_attach_mem_owning<State>(mem, is_initialized); (void)state;

    if (!is_initialized)
    {
        std::println("Owning Task: attached to unitialized memory. Initializing...");
    }

    std::println("Owning Task: Memory is initialized. Executing...");
}

void task_non_owning(void *mem)
{
    bool attached = mne_attach_mem_non_owning<State>(mem);
    if (!attached)
    {
        std::println("Non-owning Task: cannot be attached to uninitialized memory");
    }
    else
    {
        State &state = mne_get_root_struct<State>(); (void)state;
        std::println("Non-owning Task: attached to initialized memory. Executing...");
    }
}

int main()
{
    {
        std::println("Test 1: Owning task then non-owning task");

        size_t mem_size = MB(4);
        void *mem = reserve_mem(mem_size);

        task_owning(mem);
        mne_detach_mem<State>();

        task_non_owning(mem);
        mne_detach_mem<State>();

        free_mem(mem, mem_size);

        std::println("---");
    }

    {
        std::println("Test 2: Just non-owning task");

        size_t mem_size = MB(4);
        void *mem = reserve_mem(mem_size);

        task_non_owning(mem);
        mne_detach_mem<State>();

        free_mem(mem, mem_size);

        std::println("---");
    }

    {
        std::println("Test 3: Owning task twice");

        size_t mem_size = MB(4);
        void *mem = reserve_mem(mem_size);

        task_owning(mem);
        mne_detach_mem<State>();

        task_owning(mem);
        mne_detach_mem<State>();

        free_mem(mem, mem_size);

        std::println("---");
    }

    return 0;
}
