#pragma once


#include <cstddef>
#include <utility>

#include <ManagedString.hpp>
#include <NonNullPointer.hpp>


namespace FK {

/**
 * Owns a root message and context entries, newest context first. The allocator
 * must outlive the error. Moves transfer ownership without allocating.
 * Diagnostic allocation failures retain the root code and existing messages;
 * `hasIncompleteDetails()` records that some requested text could not be stored.
 */
class Error {
private:
    struct Context {
        Context(ManagedString text, Context* next) noexcept
            : m_text(std::move(text)), m_next(next) {
        }

        ManagedString m_text;
        Context* m_next;
    };


public:
    ~Error();
    Error(const Error&) = delete;
    Error& operator=(const Error&) = delete;
    Error(Error&&) noexcept;
    Error& operator=(Error&&) noexcept;


    /** Creates an allocation-free error; its root message is derived from the code. */
    static Error fromCode(Allocator& allocator, ErrorCode code) noexcept;

    /** Copies the message. If allocation fails, retains the code's fallback message. */
    static Error fromMessage(Allocator& allocator, StringView message,
                             ErrorCode code = ErrorCode::UNSPECIFIED);

    template <std::size_t N>
    static Error fromMessage(Allocator& allocator, const char (&message)[N],
                             ErrorCode code = ErrorCode::UNSPECIFIED) {
        return fromMessage(allocator, StringView::fromCString(message), code);
    }

    /** Reports allocation failure without replacing the original failure or its context. */
    ErrorCodeOr<void> addContext(StringView message);

    template <std::size_t N> ErrorCodeOr<void> addContext(const char (&message)[N]) {
        return addContext(StringView::fromCString(message));
    }

    /** Best-effort context for propagation: `return result.releaseError().withContext(text);`. */
    [[nodiscard]] Error withContext(StringView message) &&;

    template <std::size_t N> [[nodiscard]] Error withContext(const char (&message)[N]) && {
        return std::move(*this).withContext(StringView::fromCString(message));
    }


    ErrorCode code() const noexcept {
        return m_code;
    }

    bool hasIncompleteDetails() const noexcept {
        return m_incomplete_details;
    }

    StringView rootMessage() const& noexcept;
    StringView rootMessage() const&& = delete;

    /** Calls the visitor for each context, then the root message, without allocating. */
    template <class Visitor> void forEachMessage(Visitor&& visitor) const& {
        for (auto* context = m_context; context; context = context->m_next) {
            visitor(context->m_text.view());
        }

        visitor(rootMessage());
    }

    template <class Visitor> void forEachMessage(Visitor&&) const&& = delete;


private:
    Error(Allocator& allocator, ErrorCode code) noexcept;
    void clearContext() noexcept;


    NonNullPointer<Allocator> m_allocator;
    ErrorCode m_code;
    ManagedString m_message;
    Context* m_context = nullptr;
    bool m_has_message = false;
    bool m_incomplete_details = false;
};


template <class T> using AdvancedErrorOr = ErrorOr<T, Error>;

} // namespace FK
