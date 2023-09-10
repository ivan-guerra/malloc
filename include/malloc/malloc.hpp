#ifndef MEM_ALLOC_HPP_
#define MEM_ALLOC_HPP_

#include <error.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace mem {

/**
 * @brief Malloc is a memory allocation utility.
 *
 * Malloc provides an interface for the management of "heap" memory. Malloc's
 * template parameter specifies the amount of memory that will be requested
 * from the OS. It is then upto the User to allocate and free memory from this
 * pool of memory using Malloc's API.
 */
template <std::size_t N>
    requires(N > 0)
class Malloc {
   public:
    /**
     * @brief Default constructor.
     *
     * When requesting memory from the OS, Malloc will request memory in units
     * of the page size (typically 4096 bytes).
     *
     * @throws std::runtime_error When unable to acquire requested amount of
     *                            memory from the OS.
     */
    Malloc();

    /**
     * @brief Destructor
     *
     * Releases all memory acquired from the OS back to the OS. Never use
     * pointers acquired via Malloc::Alloc() after destruction!
     */
    ~Malloc();

    /* Disable copying. If we supported copying, we would end up requesting
     * memory from the OS on each copy. Not ideal. */
    Malloc(const Malloc&) = delete;
    Malloc& operator=(const Malloc&) = delete;

    Malloc(Malloc&& rhs);
    Malloc& operator=(Malloc&& rhs);

    /** @brief Return the size of the memory acquired from the OS. */
    std::size_t RegionSize() const { return region_size_; }

    /**
     * @brief Allocate a block of memory of \p size bytes.
     *
     * @param size The number of bytes to allocate. Note, Malloc may allocate
     *             more than the requested number bytes in order to support its
     *             own internal bookkeeping data structures and alignment
     *             requirements.
     * @param alignment The byte alignment of the address returned. Must be a
     *                  power of two.
     *
     * @throws std::invalid_argument When given an invalid size or alignment
     *                               argument that is not a power of two.
     * @returns A \p alignment byte aligned pointer to a chunk of memory.
     *          \p nullptr is returned if there is insufficient memory to
     *          satisfy the request.
     */
    void* Alloc(std::size_t size, std::size_t alignment = 8);

    /**
     * @brief Free a block of memory previously allocated via Alloc().
     *
     * @param block A pointer to a block of memory previously allocated via a
     *              call to Alloc().
     *
     * @throws std::runtime_error When given a \p nullptr as input or when the
     *                            the parameter pointer does not point to a
     *                            valid block of memory.
     */
    void Free(void* block);

   private:
    static const int kMemMagicNum =
        0xDEADBEEF; /**< Magic number used to mark the end of a memory block
                       header. */

    /**
     * @brief Representation of a free memory block node in a free memory list.
     */
    struct MemBlock {
        std::size_t size = 0;
        MemBlock* next = nullptr;
    };

    /**
     * @brief Header attached to each allocated memory block.
     */
    struct MemBlockHeader {
        int magic = 0;
        std::size_t size = 0;
    };

    /** @brief Return \p true if \p n is a power of two. */
    bool IsPowerOfTwo(std::size_t n) const { return n && (!(n & (n - 1))); }

    /**
     * @brief Insert \p block into a list of free blocks.
     *
     * InsertFreeMemBlock() inserts blocks by order of their address. That is,
     * the list of free memory blocks is sorted by address in ascending order.
     *
     * @param block A free memory block.
     */
    void InsertFreeMemBlock(MemBlock* block);

    /**
     * @brief Merge adjacent memory blocks in the free list.
     */
    void MergeFreeBlocks();

    std::size_t region_size_; /**< Amount of bytes requested from the OS. */
    MemBlock* mmap_start_;    /**< Start of the memory mapped region. */
    MemBlock* head_;          /**< Head of the list of free memory blocks. */
};

template <std::size_t N>
    requires(N > 0)
void Malloc<N>::InsertFreeMemBlock(MemBlock* block) {
    MemBlock dummy = {.size = 0, .next = head_};
    MemBlock* prev = &dummy;
    MemBlock* curr = head_;
    bool inserted = false;
    std::uintptr_t block_end_addr =
        reinterpret_cast<std::uintptr_t>(block) + block->size;
    while (curr) {
        std::uintptr_t curr_block_addr = reinterpret_cast<std::uintptr_t>(curr);
        if (block_end_addr <= curr_block_addr) { /* insert block before curr */
            block->next = curr;
            prev->next = block;
            inserted = true;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    if (!inserted) { /* insert at the tail of the free list */
        prev->next = block;
        block->next = nullptr;
    }

    head_ = dummy.next;
}

template <std::size_t N>
    requires(N > 0)
void Malloc<N>::MergeFreeBlocks() {
    MemBlock* curr = head_;
    while (curr->next) {
        std::uintptr_t adj_addr =
            reinterpret_cast<std::uintptr_t>(curr) + curr->size;
        std::uintptr_t next_addr = reinterpret_cast<std::uintptr_t>(curr->next);
        if (adj_addr == next_addr) { /* current and next block are adjacent */
            MemBlock* old_next = curr->next;
            curr->next = curr->next->next;
            curr->size += old_next->size;
        } else {
            curr = curr->next;
        }
    }
}

template <std::size_t N>
    requires(N > 0)
Malloc<N>::Malloc() : region_size_(N), mmap_start_(nullptr), head_(nullptr) {
    const int kPageSize = ::getpagesize();
    if (region_size_ % kPageSize) {
        region_size_ = (region_size_ / kPageSize) * kPageSize + kPageSize;
    }

    head_ = reinterpret_cast<MemBlock*>(mmap(nullptr, region_size_,
                                             PROT_READ | PROT_WRITE,
                                             MAP_ANON | MAP_PRIVATE, -1, 0));
    if (!head_) {
        throw std::runtime_error(::strerror(errno));
    }

    mmap_start_ = head_;
    head_->size = region_size_ - sizeof(MemBlock);
    head_->next = nullptr;
}

template <std::size_t N>
    requires(N > 0)
Malloc<N>::~Malloc() {
    if (mmap_start_) {
        if (munmap(mmap_start_, region_size_)) {
            ::perror("munmap");
        }
    }
}

template <std::size_t N>
    requires(N > 0)
Malloc<N>::Malloc(Malloc&& rhs)
    : region_size_(rhs.region_size_),
      mmap_start_(rhs.mmap_start_),
      head_(rhs.head_) {
    rhs.region_size_ = 0;
    rhs.mmap_start_ = nullptr;
    rhs.head_ = nullptr;
}

template <std::size_t N>
    requires(N > 0)
Malloc<N>& Malloc<N>::operator=(Malloc&& rhs) {
    if (this == &rhs) {
        return *this;
    }

    region_size_ = rhs.region_size_;
    mmap_start_ = rhs.mmap_start_;
    head_ = rhs.head_;

    rhs.region_size_ = 0;
    rhs.mmap_start_ = nullptr;
    rhs.head_ = nullptr;

    return *this;
}

template <std::size_t N>
    requires(N > 0)
void* Malloc<N>::Alloc(std::size_t size, std::size_t alignment) {
    if (size <= 0) {
        throw std::invalid_argument("size must be a positive integer");
    }
    if (alignment == 0 || !IsPowerOfTwo(alignment)) {
        throw std::invalid_argument("alignment must be a power of 2");
    }

    /* we must add additional space to accomodate the block header, alignment
     * requirement, and a byte to store the number of bytes used in alignment */
    std::size_t req_space = size + sizeof(MemBlock) + alignment + 1;

    /* dummy simplifies the splitting of the free list */
    MemBlock dummy = {.size = 0, .next = head_};
    MemBlock* prev = &dummy;
    MemBlock* curr = head_;
    while (curr) { /* taking a first fit approach */
        if (curr->size >= req_space) {
            break; /* found a large enough chunk */
        }
        prev = curr;
        curr = curr->next;
    }

    if (!curr) { /* not enough mem available, unable to satisfy request */
        return nullptr;
    }

    /* split off the user's memory chunk from the free list node */
    if (req_space < curr->size) { /* current free node is being split */
        MemBlock* split_node = reinterpret_cast<MemBlock*>(
            reinterpret_cast<char*>(curr) + req_space);
        split_node->size = curr->size - req_space;
        split_node->next = curr->next;

        prev->next = split_node;
    } else { /* current free node is being entirely consumed */
        prev->next = curr->next;
    }

    head_ = dummy.next; /* update the head of the free list */

    /* configure the block header */
    MemBlockHeader* header = reinterpret_cast<MemBlockHeader*>(curr);
    header->size = req_space - sizeof(MemBlockHeader);
    header->magic = kMemMagicNum;

    void* user_ptr = header + 1; /* user space starts just passed the header */

    /* shift the user pointer up a byte to make room for the alignment count */
    user_ptr = reinterpret_cast<char*>(user_ptr) + 1;

    /* the -1 is used to account for the alignment byte's space */
    std::size_t total_space = header->size - 1;
    std::size_t total_space_copy = header->size - 1;
    user_ptr =
        std::align(alignment, total_space - alignment, user_ptr, total_space);

    /* save how many bytes were used for alignment in the byte just before
     * user_ptr */
    uint8_t* alignment_byte_cnt_addr = reinterpret_cast<uint8_t*>(user_ptr) - 1;
    *alignment_byte_cnt_addr = total_space_copy - total_space;

    return user_ptr;
}

template <std::size_t N>
    requires(N > 0)
void Malloc<N>::Free(void* block) {
    if (!block) {
        throw std::runtime_error("cannot free NULL mem block");
    }

    uint8_t* alignment_byte_cnt_addr = reinterpret_cast<uint8_t*>(block) - 1;
    uint8_t alignment_byte_cnt = *alignment_byte_cnt_addr;

    MemBlockHeader* header = reinterpret_cast<MemBlockHeader*>(
        reinterpret_cast<char*>(block) - sizeof(MemBlockHeader) -
        alignment_byte_cnt - 1);
    if (header->magic != kMemMagicNum) {
        throw std::runtime_error("invalid mem block magic number");
    }

    MemBlock* insert_block = reinterpret_cast<MemBlock*>(header);
    insert_block->size = header->size + sizeof(MemBlockHeader);
    insert_block->next = nullptr;

    InsertFreeMemBlock(insert_block);
    MergeFreeBlocks();
}

}  // namespace mem

#endif
