//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_CrashReporter_H__
#define _Rtt_CrashReporter_H__

#ifdef __cplusplus
extern "C" {
#endif

// Breadcrumb categories for the flight recorder
enum CrashBreadcrumbType {
    kBreadcrumb_Scene = 0,    // Scene transitions
    kBreadcrumb_Texture,      // Texture load/destroy
    kBreadcrumb_Physics,      // Physics operations
    kBreadcrumb_LuaCall,      // Lua->C++ calls
    kBreadcrumb_Draw,         // Render submits
    kBreadcrumb_Memory,       // Memory operations
    kBreadcrumb_Custom,       // Custom events
    kBreadcrumb_COUNT
};

// Initialize the crash reporter. Sets the crash file path and installs
// signal handlers. If a previous crash file exists, dumps it to the log.
// crashFilePath may be NULL (breadcrumbs still go to stderr on crash).
void Rtt_CrashReporterInit(const char* crashFilePath);

// Record a breadcrumb event. Lock-free, very low overhead.
// fmt uses snprintf-style formatting. Message truncated to 119 chars.
// NOTE: NOT async-signal-safe (uses vsnprintf). Do NOT call from signal handlers.
void Rtt_BreadcrumbRecord(enum CrashBreadcrumbType type, const char* fmt, ...);

// Dump the ring buffer contents to the given fd. Uses only async-signal-safe
// functions (write). Safe to call from a signal handler.
void Rtt_BreadcrumbDump(int fd);

#ifdef __cplusplus
}
#endif

#endif // _Rtt_CrashReporter_H__
