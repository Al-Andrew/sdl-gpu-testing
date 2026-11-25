#pragma once


/* Optional CPU instrumentation. Disabled builds neither link Tracy nor evaluate arguments. */
#ifdef FK_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#define FK_PROFILE_ZONE(name) ZoneScopedN(name)
#define FK_PROFILE_FRAME() FrameMark
#define FK_PROFILE_ALLOC(pointer, size) TracyAllocN(pointer, size, "FieldKit")
#define FK_PROFILE_FREE(pointer) TracyFreeN(pointer, "FieldKit")
#define FK_PROFILE_PLOT(name, value) TracyPlot(name, value)
#else
#define FK_PROFILE_ZONE(name) ((void)0)
#define FK_PROFILE_FRAME() ((void)0)
#define FK_PROFILE_ALLOC(pointer, size) ((void)0)
#define FK_PROFILE_FREE(pointer) ((void)0)
#define FK_PROFILE_PLOT(name, value) ((void)0)
#endif
