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

template <std::size_t N>
    requires(N > 0)
class Malloc {
   public:
    Malloc();
    ~Malloc();
    Malloc(const Malloc&) = delete;
    Malloc& operator=(const Malloc&) = delete;
    Malloc(Malloc&& rhs);
    Malloc& operator=(Malloc&& rhs);

    std::size_t RegionSize() const { return region_size_; }
    void* Alloc(std::size_t size, std::size_t alignment = 8);
    void Free(void* block);
    void PrintFreeBlocks() const;

   private:
    static const int kMemMagicNum = 0xDEADBEEF;

    struct MemBlock {
        std::size_t size = 0;
        MemBlock* next = nullptr;
    };

    struct MemBlockHeader {
        int magic = 0;
        std::size_t size = 0;
    };

    bool IsPowerOfTwo(std::size_t n) const { return n && (!(n & (n - 1))); }
    void InsertFreeMemBlock(MemBlock* block);
    void MergeFreeBlocks();

    std::size_t region_size_;
    MemBlock* mmap_start_;
    MemBlock* head_;
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
        if (block_end_addr <= curr_block_addr) {
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
    if (!head_) return;

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

template <std::size_t N>
    requires(N > 0)
void Malloc<N>::PrintFreeBlocks() const {
    MemBlock* curr = head_;
    while (curr) {
        std::printf("(%zu, %p) -> ", curr->size,
                    reinterpret_cast<void*>(curr->next));
        curr = curr->next;
    }
    std::cout << "NULL" << std::endl;
}

}  // namespace mem

#endif
