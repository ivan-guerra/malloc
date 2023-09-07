#include <iostream>
#include <vector>

#include "mem_alloc/mem_alloc.hpp"

int main() {
    constexpr size_t kMemRegionBytes = 4097;

    mem::MemAlloc<kMemRegionBytes> allocator;
    allocator.PrintFreeBlocks();
    std::vector<void*> ptrs;
    for (int i = 0; i < 5; ++i) {
        ptrs.push_back(allocator.Alloc(101));
        std::cout << "ptr" << i << " = " << ptrs[i] << std::endl;
    }
    allocator.PrintFreeBlocks();

    for (auto& ptr : ptrs) {
        allocator.Free(ptr);
    }
    allocator.PrintFreeBlocks();

    return 0;
}
