#pragma once


#include <FieldKit/BuildMode.hpp>


namespace FK {

[[noreturn]] void assertionFailed(const char* expression, const char* file, int line,
                                  const char* message = nullptr) noexcept;

} // namespace FK


/** Always enabled; evaluates the condition once and the optional message only on failure. */
#define FK_VERIFY(condition, ...)                                                                  \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            ::FK::assertionFailed(#condition, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);      \
        }                                                                                          \
    } while (false)

/** Debug-only assertion. In release mode, neither the condition nor the message is evaluated. */
#if FK_MODE_DEBUG
#define FK_ASSERT(condition, ...) FK_VERIFY(condition __VA_OPT__(, ) __VA_ARGS__)
#else
#define FK_ASSERT(condition, ...) ((void)0)
#endif
