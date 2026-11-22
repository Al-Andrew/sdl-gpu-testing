#pragma once


#include <cstddef>

#include <FieldKit/ErrorOr.hpp>


namespace FK {

/**
 * Must outlive its allocations. Size is nonzero; alignment is a power of two.
 * Failure returns no storage. Deallocation receives the original size/alignment.
 */
class Allocator {
public:
    virtual ~Allocator() = default;


    virtual ErrorCodeOr<void*> allocate(std::size_t size, std::size_t alignment) noexcept = 0;
    /**
     * Resizes raw byte storage, preserving the first `min(old_size, new_size)` bytes.
     * The pointer must be a live allocation from this allocator with the supplied
     * old size and alignment. Both sizes must be nonzero; alignment cannot change.
     * Success consumes the old allocation; failure leaves it intact.
     * Only use for byte storage or objects whose lifetimes permit byte relocation.
     * Overriding is optional: the default allocates, copies, then deallocates.
     */
    virtual ErrorCodeOr<void*> reallocate(void* pointer, std::size_t old_size, std::size_t new_size,
                                          std::size_t alignment) noexcept;

    virtual void deallocate(void*, std::size_t size, std::size_t alignment) noexcept = 0;
};


class LibCppAllocator final : public Allocator {
public:
    ErrorCodeOr<void*> allocate(std::size_t, std::size_t) noexcept override;
    void deallocate(void*, std::size_t, std::size_t) noexcept override;
};

} // namespace FK
