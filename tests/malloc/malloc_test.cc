#include "malloc/malloc.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

static constexpr std::size_t kPageSize = 4096;

TEST(Malloc, ConstructAllocatorUsingRegionSizeAMultipleOfPageSize) {
    mem::Malloc<kPageSize> allocator;
    ASSERT_EQ(allocator.RegionSize(), kPageSize);
}

TEST(Malloc, ConstructAllocatorUsingRegionSizeNotAMultipleOfPageSize) {
    constexpr std::size_t kNumPages = 3;
    mem::Malloc<kPageSize * kNumPages + 1> allocator;
    ASSERT_EQ(allocator.RegionSize(), kPageSize * (kNumPages + 1));
}

TEST(Malloc, RegionSizeReturnsAllocatedSizePostMoveConstruction) {
    mem::Malloc<kPageSize> allocator1;
    ASSERT_EQ(allocator1.RegionSize(), kPageSize);

    mem::Malloc<kPageSize> allocator2(std::move(allocator1));
    ASSERT_EQ(allocator1.RegionSize(), 0);
    ASSERT_EQ(allocator2.RegionSize(), kPageSize);
}

TEST(Malloc, RegionSizeReturnsAllocatedSizePostMoveAssignment) {
    mem::Malloc<kPageSize> allocator1;
    ASSERT_EQ(allocator1.RegionSize(), kPageSize);

    mem::Malloc<kPageSize> allocator2 = std::move(allocator1);
    ASSERT_EQ(allocator1.RegionSize(), 0);
    ASSERT_EQ(allocator2.RegionSize(), kPageSize);
}

TEST(Malloc, AllocThrowsInvalidArgumentWhenSizeIsZero) {
    mem::Malloc<kPageSize> allocator;
    ASSERT_THROW(allocator.Alloc(0), std::invalid_argument);
}

TEST(Malloc, AllocThrowsInvalidArgumentWhenAlignmentIsZero) {
    mem::Malloc<kPageSize> allocator;
    ASSERT_THROW(allocator.Alloc(1024, 0), std::invalid_argument);
}

TEST(Malloc, AllocThrowsInvalidArgumentWhenAlignmentIsNotAPowerOfTwo) {
    mem::Malloc<kPageSize> allocator;
    ASSERT_THROW(allocator.Alloc(1024, 7), std::invalid_argument);
}

TEST(Malloc, AllocReturnsNullptrWhenRequestExceedsAvailableMem) {
    mem::Malloc<kPageSize> allocator;
    /* should return nullptr because the allocator uses part of the memory
     * pool to do bookkeeping therefore the amount of mem in the pool is
     * slightly less than that requested via the template param */
    ASSERT_EQ(allocator.Alloc(kPageSize), nullptr);
}

TEST(Malloc, AllocReturnsAlignedAddresses) {
    mem::Malloc<kPageSize> allocator;

    const std::size_t kUnalignedRequest = 100;
    const std::size_t kAlignments[] = {8, 16, 32, 64, 128};
    for (const std::size_t alignment : kAlignments) {
        void* ptr = allocator.Alloc(kUnalignedRequest, alignment);
        ASSERT_NE(ptr, nullptr);
        ASSERT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
        allocator.Free(ptr);
    }
}

TEST(Malloc, FreeThrowsRuntimeErrorWhenGivenNullptr) {
    mem::Malloc<kPageSize> allocator;
    ASSERT_THROW(allocator.Free(nullptr), std::runtime_error);
}

TEST(Malloc, FreeThrowsRuntimeErrorWhenGivenInvalidAndAddressablePtr) {
    std::vector<char> ptr(256, 0);
    mem::Malloc<kPageSize> allocator;
    ASSERT_THROW(allocator.Free(ptr.data() + ptr.size() - 1),
                 std::runtime_error);
}

TEST(Malloc, FreeReleasesAllocatedMemorySuccessfully) {
    mem::Malloc<kPageSize> allocator;
    void* ptr = allocator.Alloc(1024);
    ASSERT_NE(ptr, nullptr);
    allocator.Free(ptr);
}
