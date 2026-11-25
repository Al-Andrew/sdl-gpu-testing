#include <cstring>
#include <new>

#include <FieldKit/Allocator.hpp>
#include <FieldKit/Profiling.hpp>


namespace FK {

ErrorCodeOr<void*> Allocator::reallocate(void* pointer, std::size_t old_size, std::size_t new_size,
                                         std::size_t alignment) noexcept {
    if (!pointer || !old_size || !new_size || !alignment || (alignment & (alignment - 1))) {
        return ErrorCode::INVALID_ARGUMENT;
    }

    if (old_size == new_size) {
        return pointer;
    }

    auto allocation = allocate(new_size, alignment);
    if (allocation.isError()) {
        return allocation.releaseError();
    }

    auto* replacement = allocation.releaseValue();
    const auto copy_size = old_size < new_size ? old_size : new_size;
    std::memcpy(replacement, pointer, copy_size);
    deallocate(pointer, old_size, alignment);

    return replacement;
}


ErrorCodeOr<void*> LibCppAllocator::allocate(std::size_t size, std::size_t alignment) noexcept {
    if (!size || !alignment || (alignment & (alignment - 1))) {
        return ErrorCode::INVALID_ARGUMENT;
    }

    void* p = alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__
                  ? ::operator new(size, std::align_val_t(alignment), std::nothrow)
                  : ::operator new(size, std::nothrow);

    if (!p) {
        return ErrorCode::OUT_OF_MEMORY;
    }

    FK_PROFILE_ALLOC(p, size);

    return p;
}

void LibCppAllocator::deallocate(void* p, std::size_t, std::size_t alignment) noexcept {
    if (p) {
        FK_PROFILE_FREE(p);
    }

    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        ::operator delete(p, std::align_val_t(alignment));
    } else {
        ::operator delete(p);
    }
}

} // namespace FK
