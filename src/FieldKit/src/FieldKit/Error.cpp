#include <memory>
#include <utility>

#include <FieldKit/Error.hpp>


namespace FK {

Error::Error(Allocator& allocator, ErrorCode code) noexcept
    : m_allocator(allocator), m_code(code), m_message(allocator) {
}

Error::~Error() {
    clearContext();
}

Error::Error(Error&& other) noexcept
    : m_allocator(other.m_allocator), m_code(other.m_code), m_message(std::move(other.m_message)),
      m_context(std::exchange(other.m_context, nullptr)),
      m_has_message(std::exchange(other.m_has_message, false)),
      m_incomplete_details(std::exchange(other.m_incomplete_details, false)) {
}

Error& Error::operator=(Error&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    clearContext();
    m_message = std::move(other.m_message);
    m_allocator = other.m_allocator;
    m_code = other.m_code;
    m_context = std::exchange(other.m_context, nullptr);
    m_has_message = std::exchange(other.m_has_message, false);
    m_incomplete_details = std::exchange(other.m_incomplete_details, false);

    return *this;
}


Error Error::fromCode(Allocator& allocator, ErrorCode code) noexcept {
    return Error(allocator, code);
}

Error Error::fromMessage(Allocator& allocator, StringView message, ErrorCode code) {
    Error error(allocator, code);
    auto copied = error.m_message.assign(message);
    error.m_has_message = !copied.isError();
    error.m_incomplete_details = copied.isError();

    return error;
}

ErrorCodeOr<void> Error::addContext(StringView message) {
    auto copied = ManagedString::from(*m_allocator, message);
    if (copied.isError()) {
        m_incomplete_details = true;
        return copied.releaseError();
    }

    auto allocation = m_allocator->allocate(sizeof(Context), alignof(Context));
    if (allocation.isError()) {
        m_incomplete_details = true;
        return allocation.releaseError();
    }

    /* Publish the node only after both allocations succeed; prior context remains intact. */
    auto* context = static_cast<Context*>(allocation.releaseValue());
    std::construct_at(context, copied.releaseValue(), m_context);
    m_context = context;

    return {};
}

Error Error::withContext(StringView message) && {
    /* Intentionally best-effort: addContext records loss, and the root failure survives. */
    (void)addContext(message);

    return std::move(*this);
}


StringView Error::rootMessage() const& noexcept {
    if (m_has_message) {
        return m_message.view();
    }

    switch (m_code) {
    case ErrorCode::OUT_OF_MEMORY: {
        return StringView::fromCString("Out of memory");
    }
    case ErrorCode::INVALID_ARGUMENT: {
        return StringView::fromCString("Invalid argument");
    }
    case ErrorCode::OUT_OF_RANGE: {
        return StringView::fromCString("Out of range");
    }
    case ErrorCode::OVERFLOW: {
        return StringView::fromCString("Overflow");
    }
    case ErrorCode::UNSPECIFIED: {
        return StringView::fromCString("Operation failed");
    }
    }

    return StringView::fromCString("Unknown error");
}


void Error::clearContext() noexcept {
    /* Iterative destruction keeps long propagation chains off the call stack. */
    while (m_context) {
        auto* context = m_context;
        m_context = context->m_next;
        std::destroy_at(context);
        m_allocator->deallocate(context, sizeof(Context), alignof(Context));
    }
}

} // namespace FK
