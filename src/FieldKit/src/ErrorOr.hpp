#pragma once


#include <memory>
#include <type_traits>
#include <utility>

#include <Assertions.hpp>
#include <BasicTypes.hpp>


namespace FK {

enum class ErrorCode : u8 { UNSPECIFIED, OUT_OF_MEMORY, INVALID_ARGUMENT, OUT_OF_RANGE, OVERFLOW };

/**
 * A move-only value or error. `E` may be an application enum or an owning error type.
 * Payloads must have nonthrowing moves and destructors, so changing alternatives
 * cannot leave the union without an active member. Construction may still throw.
 *
 * `T` and `E` must differ to keep implicit success/error construction unambiguous.
 *
 *
 * NOTE(aaldea): clang-tidy 23.1.4 counts enum variants wrong nesinside templates
 *
 */
// NOLINTNEXTLINE(bugprone-tagged-union-member-count)
template <class T, class E> class [[nodiscard]] ErrorOr {
private:
    struct Empty {};
    using StoredValue = std::conditional_t<std::is_void_v<T>, Empty, T>;

    static_assert(!std::is_same_v<T, E>, "Value and error types must differ");
    static_assert(std::is_object_v<StoredValue> && std::is_object_v<E>);
    static_assert(std::is_nothrow_move_constructible_v<StoredValue> &&
                  std::is_nothrow_move_constructible_v<E>);
    static_assert(std::is_nothrow_destructible_v<StoredValue> && std::is_nothrow_destructible_v<E>);

    enum class State : u8 { VALUE, ERROR };

    union Storage {
        Storage() {
        }
        ~Storage() {
        }

        StoredValue m_value;
        E m_error;
    };


public:
    ErrorOr() noexcept
        requires(std::is_void_v<T>)
        : m_state(State::VALUE) {
        std::construct_at(std::addressof(m_storage.m_value));
    }

    ErrorOr(StoredValue value) noexcept
        requires(!std::is_void_v<T>)
        : m_state(State::VALUE) {
        std::construct_at(std::addressof(m_storage.m_value), std::move(value));
    }

    ErrorOr(E error) noexcept : m_state(State::ERROR) {
        std::construct_at(std::addressof(m_storage.m_error), std::move(error));
    }

    ErrorOr(const ErrorOr&) = delete;
    ErrorOr& operator=(const ErrorOr&) = delete;

    ErrorOr(ErrorOr&& other) noexcept : m_state(other.m_state) {
        constructFrom(other);
    }

    ErrorOr& operator=(ErrorOr&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        destroyActiveMember();
        m_state = other.m_state;
        constructFrom(other);

        return *this;
    }

    ~ErrorOr() {
        destroyActiveMember();
    }


    bool isError() const noexcept {
        return m_state == State::ERROR;
    }

    explicit operator bool() const noexcept {
        return !isError();
    }


    StoredValue& value() &
        requires(!std::is_void_v<T>)
    {
        FK_VERIFY(!isError());

        return m_storage.m_value;
    }

    const StoredValue& value() const&
        requires(!std::is_void_v<T>)
    {
        FK_VERIFY(!isError());

        return m_storage.m_value;
    }

    StoredValue value() &&
        requires(!std::is_void_v<T>)
    {
        return releaseValue();
    }

    void value() const
        requires(std::is_void_v<T>)
    {
        FK_VERIFY(!isError());
    }

    E& error() & {
        FK_VERIFY(isError());

        return m_storage.m_error;
    }

    const E& error() const& {
        FK_VERIFY(isError());

        return m_storage.m_error;
    }

    E error() && {
        return releaseError();
    }


    /** Moves out the payload; the same alternative remains active in a moved-from state. */
    T releaseValue() noexcept {
        FK_VERIFY(!isError());

        if constexpr (!std::is_void_v<T>) {
            return std::move(m_storage.m_value);
        }
    }

    /** Like `releaseValue`, this does not destroy the member or change the discriminator. */
    E releaseError() noexcept {
        FK_VERIFY(isError());

        return std::move(m_storage.m_error);
    }


private:
    void constructFrom(ErrorOr& other) noexcept {
        /*
         * Storage's empty constructor starts no member lifetime. Explicit construction
         * starts exactly the selected member, including for nontrivial payloads.
         * Moving does not end the source member's lifetime; its destructor still runs.
         */
        if (isError()) {
            std::construct_at(std::addressof(m_storage.m_error),
                              std::move(other.m_storage.m_error));
        } else {
            std::construct_at(std::addressof(m_storage.m_value),
                              std::move(other.m_storage.m_value));
        }
    }

    void destroyActiveMember() noexcept {
        /* Only the active member may be destroyed; Storage's destructor does nothing. */
        if (isError()) {
            std::destroy_at(std::addressof(m_storage.m_error));
        } else {
            std::destroy_at(std::addressof(m_storage.m_value));
        }
    }


    Storage m_storage;
    State m_state;
};


template <class T> using ErrorCodeOr = ErrorOr<T, ErrorCode>;

} // namespace FK
