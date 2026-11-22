#pragma once


#include <climits>
#include <cstdint>
#include <limits>


namespace FK {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using f32 = float;
using f64 = double;


/* Reject unsupported floating-point representations rather than silently misnaming them. */
static_assert(sizeof(f32) * CHAR_BIT == 32 && std::numeric_limits<f32>::is_iec559 &&
              std::numeric_limits<f32>::digits == 24);
static_assert(sizeof(f64) * CHAR_BIT == 64 && std::numeric_limits<f64>::is_iec559 &&
              std::numeric_limits<f64>::digits == 53);

} // namespace FK
