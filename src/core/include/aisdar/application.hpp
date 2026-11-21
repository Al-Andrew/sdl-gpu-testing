#pragma once

#include <string_view>

namespace aisdar {
[[nodiscard]] std::string_view application_name() noexcept;
[[nodiscard]] std::string_view build_version() noexcept;
}
