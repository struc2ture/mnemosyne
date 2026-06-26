# Mnemosyne

Memory allocator backed by a monolithic arena with relative pointers and hot-swappable underlying memory.

This library has two components: the notion of relative pointers that are dereferenced with respect to the base address of an arena and a basic malloc-style allocator with the ability to free and reuse memory.

This is an exploratory project meant as an experiment, not a production-ready library.

## Example

There are 6 examples in the `examples/` folder. The best introduction is [examples/d_alloc_reattach.cpp](examples/d_alloc_reattach.cpp).

## Relocatable arenas and relative pointers

The basic usage is to allocate a larger zero-initialized block of memory using mmap or anything else, encode the size of that block into the first 8 bytes of that block, then call `mne_attach_mem_owning`,passing the address of that memory block. After that function call, all allocations and dereferences will be made on that arena. At any point, the arena can be detached and reattached to that block of memory. Similarly, the same memory contents could be copied into a different block of memory, and, after reattaching to the new base address, the allocator and relative pointers will continue working seamlessly.

Library functions are templated on an arbitrary, user-provided struct (e.g. `mne_attach_mem_owning<State>`). It's meant to act as the root struct of all allocations on that arena. The root struct is stored in the same arena, so everything, including the access to the root level state is reproducible just from the base address.

There can be multiple independent arenas on different root struct types. For example, you can have `GameState`, with `RPtr<GameState, ValueT>` pointers always being dereferenced with respect to arena initialized with `mne_attach_mem_owning<GameState>`; and `RuntimeState` with `RPtr<RuntimeState, ValueT>` with respect to arena initialized with `mne_attach_mem_owning<RuntimeState>`.

An example of how something like this could be used is for persistent state of a game. The whole state can be dumped to disk and reloaded seamlessly, even across process boundaries.

Relative pointers have the same notation as regular pointers and support operators such as `*`, `->`, `[]`. A relative pointer with an offset = 0 is treated as a NULL pointer.

## The allocator

The allocator itself traverses a list of allocated blocks, looking for a free block. If there's no free block available, it creates a new block by pushing a size onto the arena. If a free block gets reused, it gets split, given there's enough memory for the new block's meta data. Upon freeing, the freed block gets coalesced with the previous and the following blocks, so it could serve a larger allocation.

## Usage

The whole library is in one header file, [mnemosyne.h](lib/mnemosyne.h).

The project using this needs to be compiled with `-std=c++23` and `-fno-strict-aliasing`. (Have not tested otherwise).

API functions are prefixed with `mne_`. None of the internal functions and structs are hidden, but all are placed in `mne_internals` namespace.

There are two functions for attaching memory: `mne_attach_mem_owning` and `mne_attach_mem_non_owning`. The `non_owning` version will not attach if the arena is not already initialized and in use. It is meant to be used when a program is a secondary tool accessing the same memory space, that is not meant to initialize it. An arena 

Allocations are done with `mne_alloc<ArenaT, ValueT>` and `mne_free<ArenaT, ValueT>`.

All allocated blocks are aligned to max_align_t of the machine. Alignments wider than that are not supported for now.

Not thread-safe. No allocations on the same arena should be made from different threads without an external locking mechanism. However, it is very natural with this approach to create a separate arena for each thread.

## Further improvements

* alligned_alloc for alignments > max_align_t (for SIMD, etc.); keeping track of alignment on block reuse
    * Or: making the regular alloc always alignment-aware based on templated type
* Allocation hierarchy by size (small, large) with different ways of block bookkeeping for different sizes
    * E.g. for small allocations, round up to a size-class -> no O(n) list traversal;
    * for large allocations: pages, etc.
* Reserve and commit memory by pages. Release memory back to the OS.
* Thread-safety for alloc/free operations when different threads are using the same arena
* Accounting for strict aliasing rules, using placement new and std::launder, etc.
* Safe containers w/ templates and bound-checking: fixed_array, vector - so it can be built without -fno-strict-aliasing
* Safe counted strings on arena
* Some kind of basic and slow address sanitzation: Checking memory access into an arena at runtime. Compile out with a flag.
* (Micro improvement) If last block is free but is not big enough, expand its size.
* relloc, calloc

Most of the above are stretch goals. The most likely next extension will be to play with bound checked containers and strings based on arena-typed relative pointers.
