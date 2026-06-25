# Mnemosyne

Memory allocator backed by a monolithic arena with relative pointers and hot-swappable underlying memory.

## Interface

`arena_init` in `mem.h` takes a pointer to a preallocated memory block, with the first 8 bytes being the capacity of the block.

`arena_attach` takes a pointer to the block of memory and swaps the base address of the arena to the new one.

Everything is templated on ArenaT, so there can be multiple arena/memory block contexts, one for each Arena type.

## Example

[An example of basic usage](examples/d_alloc_reattach.cpp)
