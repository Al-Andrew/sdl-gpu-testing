#pragma once


#include <cstddef>
#include <type_traits>

#include <FieldKit/Assertions.hpp>


namespace FK {

class StringView {
public:
    constexpr StringView() = default;

    /** The caller guarantees `size` readable bytes. Null is allowed only for empty views. */
    constexpr StringView(const char* data, std::size_t size)
        : m_data(data ? data : ""), m_size(size) {
        FK_ASSERT(data || !size, "A nonempty view requires readable storage");
    }


    /** Pointer inputs are scanned; the array overload below avoids a runtime length scan. */
    template <class Pointer>
        requires(std::is_same_v<Pointer, const char*> || std::is_same_v<Pointer, char*>)
    static StringView fromCString(const Pointer& text) {
        FK_ASSERT(text, "A C string must not be null");

        std::size_t length = 0;
        while (text[length]) {
            ++length;
        }

        return {text, length};
    }

    /**
     * For literals or exact-sized terminated arrays: all `N - 1` bytes are retained,
     * including embedded NULs. This is not a scan of a partially filled C buffer.
     */
    template <std::size_t N> static constexpr StringView fromCString(const char (&text)[N]) {
        FK_ASSERT(text[N - 1] == '\0', "A literal array must end with NUL");

        return {text, N - 1};
    }


    constexpr std::size_t size() const noexcept {
        return m_size;
    }

    constexpr bool isEmpty() const noexcept {
        return !m_size;
    }


    constexpr const char& operator[](std::size_t index) const noexcept {
        FK_ASSERT(index < m_size, "StringView index is out of bounds");

        return m_data[index];
    }

    constexpr const char& at(std::size_t index) const noexcept {
        return (*this)[index];
    }

    constexpr StringView viewSubstringAt(std::size_t offset, std::size_t length) const noexcept {
        FK_ASSERT(offset <= m_size && length <= m_size - offset,
                  "StringView substring is out of bounds");

        return {m_data + offset, length};
    }

    constexpr StringView viewSubstringFrom(std::size_t offset) const noexcept {
        FK_ASSERT(offset <= m_size, "StringView substring offset is out of bounds");

        return {m_data + offset, m_size - offset};
    }


    friend constexpr bool operator==(StringView a, StringView b) noexcept {
        if (a.m_size != b.m_size) {
            return false;
        }

        for (std::size_t i = 0; i < a.m_size; ++i) {
            if (a.m_data[i] != b.m_data[i]) {
                return false;
            }
        }

        return true;
    }

    bool startsWith(StringView prefix) const noexcept {
        return prefix.m_size <= m_size && StringView(m_data, prefix.m_size) == prefix;
    }

    bool endsWith(StringView suffix) const noexcept {
        return suffix.m_size <= m_size &&
               StringView(m_data + m_size - suffix.m_size, suffix.m_size) == suffix;
    }


    StringView trimAsciiWhitespace() const noexcept {
        auto space = [](char c) {
            return c == ' ' || (c >= '\t' && c <= '\r');
        };

        std::size_t first = 0;
        std::size_t last = m_size;
        while (first < last && space(m_data[first])) {
            ++first;
        }
        while (last > first && space(m_data[last - 1])) {
            --last;
        }

        return {m_data + first, last - first};
    }


private:
    const char* m_data = "";
    std::size_t m_size = 0;
};

} // namespace FK
