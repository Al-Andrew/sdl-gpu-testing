#pragma once


#include <FieldKit/Allocator.hpp>
#include <FieldKit/NonNullPointer.hpp>
#include <FieldKit/StringView.hpp>


namespace FK {

class ManagedString {
public:
    explicit ManagedString(Allocator& allocator) noexcept : m_allocator(allocator) {
    }

    ~ManagedString();
    ManagedString(const ManagedString&) = delete;
    ManagedString& operator=(const ManagedString&) = delete;
    ManagedString(ManagedString&&) noexcept;
    ManagedString& operator=(ManagedString&&) noexcept;


    static ErrorCodeOr<ManagedString> from(Allocator&, StringView);

    ErrorCodeOr<ManagedString> clone(Allocator& allocator) const {
        return from(allocator, view());
    }


    ErrorCodeOr<void> reserve(std::size_t capacity);
    ErrorCodeOr<void> append(StringView);
    ErrorCodeOr<void> assign(StringView);
    void clear() noexcept;


    std::size_t size() const noexcept {
        return m_size;
    }

    std::size_t capacity() const noexcept {
        return m_capacity;
    }

    bool isEmpty() const noexcept {
        return m_size == 0;
    }

    Allocator& allocator() const noexcept {
        return *m_allocator;
    }


    const char* cStr() const& noexcept {
        return m_data ? m_data : "";
    }


    const char* cStr() const&& = delete;

    StringView view() const& noexcept {
        return {cStr(), m_size};
    }

    StringView view() const&& = delete;


private:
    NonNullPointer<Allocator> m_allocator;
    char* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
};

} // namespace FK
