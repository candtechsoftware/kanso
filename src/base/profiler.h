#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
// Define empty macros when Tracy is disabled
#define ZoneScoped
#define ZoneScopedN(name)
#define ZoneScopedC(color)
#define ZoneScopedNC(name, color)
#define ZoneText(text, size)
#define ZoneName(text, size)
#define FrameMark
#define FrameMarkNamed(name)
#define FrameMarkStart(name)
#define FrameMarkEnd(name)
#define TracyLockable(type, varname) type varname
#define TracyLockableN(type, varname, desc) type varname
#define TracySharedLockable(type, varname) type varname
#define TracySharedLockableN(type, varname, desc) type varname
#define LockableBase(type) type
#define SharedLockableBase(type) type
#define LockMark(varname)
#define TracyPlot(name, val)
#define TracyPlotConfig(name, type)
#define TracyMessage(txt, size)
#define TracyMessageL(txt)
#define TracyAlloc(ptr, size)
#define TracyFree(ptr)
#define TracyAllocN(ptr, size, name)
#define TracyFreeN(ptr, name)
#define TracyAllocS(ptr, size, depth)
#define TracyFreeS(ptr, depth)
#define TracyAllocNS(ptr, size, depth, name)
#define TracyFreeNS(ptr, depth, name)
#endif