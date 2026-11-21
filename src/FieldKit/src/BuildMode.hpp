#pragma once


#include <BasicTypes.hpp>


#if !defined(FK_MODE_DEBUG) || !defined(FK_MODE_RELEASE)
#error "Link the FK::FieldKit CMake target to obtain the build mode definitions"
#endif

#if (FK_MODE_DEBUG != 0 && FK_MODE_DEBUG != 1) ||                                                  \
    (FK_MODE_RELEASE != 0 && FK_MODE_RELEASE != 1) || (FK_MODE_DEBUG + FK_MODE_RELEASE != 1)
#error "Exactly one FieldKit build mode must be enabled"
#endif


namespace FK {

enum class BuildMode : u8 { DEBUG, RELEASE };

inline constexpr BuildMode BUILD_MODE = FK_MODE_DEBUG ? BuildMode::DEBUG : BuildMode::RELEASE;

} // namespace FK
