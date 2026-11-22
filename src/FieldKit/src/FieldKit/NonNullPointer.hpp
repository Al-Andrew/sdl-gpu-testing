#pragma once


#include <memory>


namespace FK {

/**
 * A rebindable, non-owning pointer constructed from a reference. Copies and moves
 * retain the source binding; neither can produce a null state. The pointee must
 * outlive every use. No allocation, reference counting, or dereference checks.
 */
template <class T> class NonNullPointer {
public:
    explicit constexpr NonNullPointer(T& object) noexcept : m_pointer(std::addressof(object)) {
    }


    constexpr T& operator*() const noexcept {
        return *m_pointer;
    }

    constexpr T* operator->() const noexcept {
        return m_pointer;
    }


private:
    T* m_pointer;
};

} // namespace FK
