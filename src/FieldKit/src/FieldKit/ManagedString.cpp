#include <cstring>
#include <limits>
#include <utility>

#include <FieldKit/ManagedString.hpp>


namespace FK {

ManagedString::~ManagedString() {
    if (m_data) {
        m_allocator->deallocate(m_data, m_capacity + 1, alignof(char));
    }
}

ManagedString::ManagedString(ManagedString&& other) noexcept
    : m_allocator(other.m_allocator), m_data(std::exchange(other.m_data, nullptr)),
      m_size(std::exchange(other.m_size, 0)), m_capacity(std::exchange(other.m_capacity, 0)) {
}

ManagedString& ManagedString::operator=(ManagedString&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (m_data) {
        m_allocator->deallocate(m_data, m_capacity + 1, alignof(char));
    }
    m_allocator = other.m_allocator;
    m_data = std::exchange(other.m_data, nullptr);
    m_size = std::exchange(other.m_size, 0);
    m_capacity = std::exchange(other.m_capacity, 0);
    return *this;
}


ErrorCodeOr<ManagedString> ManagedString::from(Allocator& allocator, StringView text) {
    ManagedString result(allocator);
    auto status = result.append(text);
    if (!status) {
        return status.error();
    }
    return result;
}


ErrorCodeOr<void> ManagedString::reserve(std::size_t capacity) {
    if (capacity <= m_capacity) {
        return {};
    }
    if (capacity == std::numeric_limits<std::size_t>::max()) {
        return ErrorCode::OVERFLOW;
    }

    auto allocation =
        m_data ? m_allocator->reallocate(m_data, m_capacity + 1, capacity + 1, alignof(char))
               : m_allocator->allocate(capacity + 1, alignof(char));
    if (allocation.isError()) {
        return allocation.releaseError();
    }

    auto* next = static_cast<char*>(allocation.releaseValue());
    next[m_size] = '\0';

    m_data = next;
    m_capacity = capacity;
    return {};
}

ErrorCodeOr<void> ManagedString::append(StringView text) {
    constexpr auto MAX_SIZE = std::numeric_limits<std::size_t>::max() - 1;
    if (text.size() > MAX_SIZE - m_size) {
        return ErrorCode::OVERFLOW;
    }
    const auto next_size = m_size + text.size();
    if (next_size > m_capacity) {
        /**
         * Copy the source before releasing storage: self-append and overlapping views are valid.
         */
        auto grown = m_capacity > MAX_SIZE / 2 ? MAX_SIZE : m_capacity * 2;
        auto capacity = next_size > grown ? next_size : grown;
        auto allocation = m_allocator->allocate(capacity + 1, alignof(char));
        if (!allocation) {
            return allocation.error();
        }
        auto* next = static_cast<char*>(allocation.value());
        if (m_size) {
            std::memcpy(next, m_data, m_size);
        }
        if (text.size()) {
            std::memcpy(next + m_size, &text[0], text.size());
        }
        next[next_size] = '\0';
        if (m_data) {
            m_allocator->deallocate(m_data, m_capacity + 1, alignof(char));
        }
        m_data = next;
        m_capacity = capacity;
    } else if (text.size()) {
        std::memmove(m_data + m_size, &text[0], text.size());
        m_data[next_size] = '\0';
    }
    m_size = next_size;
    return {};
}

ErrorCodeOr<void> ManagedString::assign(StringView text) {
    if (text.size() <= m_capacity) {
        if (text.size()) {
            std::memmove(m_data, &text[0], text.size());
        }
        m_size = text.size();
        if (m_data) {
            m_data[m_size] = '\0';
        }
        return {};
    }
    auto replacement = from(*m_allocator, text);
    if (!replacement) {
        return replacement.error();
    }
    *this = std::move(replacement).value();
    return {};
}

void ManagedString::clear() noexcept {
    m_size = 0;
    if (m_data) {
        m_data[0] = '\0';
    }
}

} // namespace FK
