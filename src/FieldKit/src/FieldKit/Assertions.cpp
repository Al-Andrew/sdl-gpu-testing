#include <cstdio>
#include <cstdlib>

#include <FieldKit/Assertions.hpp>


namespace FK {

[[noreturn]] void assertionFailed(const char* expression, const char* file, int line,
                                  const char* message) noexcept {
    std::fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, expression);
    if (message) {
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
    }

    std::abort();
}

} // namespace FK
