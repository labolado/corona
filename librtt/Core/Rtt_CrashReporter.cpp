//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_CrashReporter.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#if defined(__APPLE__)
  #include <mach/mach_time.h>
  #include <sys/stat.h>
#elif defined(__ANDROID__) || defined(ANDROID)
  #include <android/log.h>
  #include <sys/stat.h>
#else
  #include <sys/stat.h>
#endif

// Portable atomic operations
#if defined(__cplusplus) && __cplusplus >= 201103L
  #include <atomic>
  #define ATOMIC_UINT32 std::atomic<uint32_t>
  #define ATOMIC_LOAD(x) (x).load(std::memory_order_relaxed)
  #define ATOMIC_FETCH_ADD(x, v) (x).fetch_add(v, std::memory_order_relaxed)
#else
  #define ATOMIC_UINT32 volatile uint32_t
  #define ATOMIC_LOAD(x) __atomic_load_n(&(x), __ATOMIC_RELAXED)
  #define ATOMIC_FETCH_ADD(x, v) __atomic_fetch_add(&(x), v, __ATOMIC_RELAXED)
#endif

// ----------------------------------------------------------------------------
// Ring buffer configuration
// ----------------------------------------------------------------------------

#define BREADCRUMB_RING_SIZE 1024
#define BREADCRUMB_MSG_LEN   120

struct BreadcrumbEntry {
    uint32_t timestampMs;
    uint8_t  type;      // CrashBreadcrumbType
    char     msg[BREADCRUMB_MSG_LEN];
};

// Static ring buffer - no heap allocation
static BreadcrumbEntry sRing[BREADCRUMB_RING_SIZE];
static ATOMIC_UINT32   sWriteIndex
#if defined(__cplusplus) && __cplusplus >= 201103L
    {0}
#endif
    ;

// Crash file path (set once at init)
static char sCrashFilePath[512];

// Original signal handlers for chaining
static struct sigaction sOldSIGSEGV;
static struct sigaction sOldSIGABRT;
static struct sigaction sOldSIGBUS;
static struct sigaction sOldSIGILL;
static struct sigaction sOldSIGFPE;

// Category names for dump output
static const char* sBreadcrumbNames[] = {
    "SCENE",
    "TEXTURE",
    "PHYSICS",
    "LUA",
    "DRAW",
    "MEMORY",
    "CUSTOM"
};
static_assert(
    sizeof(sBreadcrumbNames) / sizeof(sBreadcrumbNames[0]) == kBreadcrumb_COUNT,
    "sBreadcrumbNames must match CrashBreadcrumbType enum"
);

// ----------------------------------------------------------------------------
// Async-signal-safe helpers (no malloc, no printf, no fwrite)
// ----------------------------------------------------------------------------

// Write a string to fd (signal-safe)
static void safe_write_str(int fd, const char* s)
{
    if (!s) return;
    size_t len = 0;
    while (s[len]) len++;
    // Ignore return: best-effort in signal handler
    (void)write(fd, s, len);
}

// Write uint32 as decimal to fd (signal-safe)
static void safe_write_uint(int fd, uint32_t val)
{
    char buf[12];
    int pos = 11;
    buf[pos] = '\0';
    if (val == 0) {
        buf[--pos] = '0';
    } else {
        while (val > 0) {
            buf[--pos] = '0' + (val % 10);
            val /= 10;
        }
    }
    safe_write_str(fd, &buf[pos]);
}

// Get monotonic time in milliseconds
static uint32_t get_time_ms(void)
{
#if defined(__APPLE__)
    static mach_timebase_info_data_t sTimebase = {0, 0};
    if (sTimebase.denom == 0) {
        mach_timebase_info(&sTimebase);
    }
    uint64_t t = mach_absolute_time();
    return (uint32_t)((t * sTimebase.numer / sTimebase.denom) / 1000000ULL);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// ----------------------------------------------------------------------------
// Breadcrumb recording (lock-free)
// ----------------------------------------------------------------------------

void Rtt_BreadcrumbRecord(enum CrashBreadcrumbType type, const char* fmt, ...)
{
    uint32_t idx = ATOMIC_FETCH_ADD(sWriteIndex, 1u);
    uint32_t slot = idx & (BREADCRUMB_RING_SIZE - 1); // power-of-2 mask

    BreadcrumbEntry* e = &sRing[slot];
    e->timestampMs = get_time_ms();
    e->type = (uint8_t)type;

    va_list args;
    va_start(args, fmt);
    vsnprintf(e->msg, BREADCRUMB_MSG_LEN, fmt, args);
    va_end(args);
    e->msg[BREADCRUMB_MSG_LEN - 1] = '\0';
}

// ----------------------------------------------------------------------------
// Breadcrumb dump (async-signal-safe)
// ----------------------------------------------------------------------------

void Rtt_BreadcrumbDump(int fd)
{
    uint32_t totalWritten = ATOMIC_LOAD(sWriteIndex);
    if (totalWritten == 0) {
        safe_write_str(fd, "[CrashReporter] No breadcrumbs recorded.\n");
        return;
    }

    uint32_t count = totalWritten < BREADCRUMB_RING_SIZE ? totalWritten : BREADCRUMB_RING_SIZE;
    uint32_t start = totalWritten <= BREADCRUMB_RING_SIZE ? 0 : (totalWritten & (BREADCRUMB_RING_SIZE - 1));

    safe_write_str(fd, "\n=== CRASH FLIGHT RECORDER ===\n");
    safe_write_str(fd, "Total events: ");
    safe_write_uint(fd, totalWritten);
    safe_write_str(fd, ", showing last ");
    safe_write_uint(fd, count);
    safe_write_str(fd, "\n\n");

    for (uint32_t i = 0; i < count; i++) {
        uint32_t slot = (start + i) & (BREADCRUMB_RING_SIZE - 1);
        const BreadcrumbEntry* e = &sRing[slot];

        // Format: [timestamp_ms] CATEGORY: message
        safe_write_str(fd, "[");
        safe_write_uint(fd, e->timestampMs);
        safe_write_str(fd, "] ");
        if (e->type < kBreadcrumb_COUNT) {
            safe_write_str(fd, sBreadcrumbNames[e->type]);
        } else {
            safe_write_str(fd, "???");
        }
        safe_write_str(fd, ": ");
        safe_write_str(fd, e->msg);
        safe_write_str(fd, "\n");
    }

    safe_write_str(fd, "=== END FLIGHT RECORDER ===\n\n");
}

// ----------------------------------------------------------------------------
// Signal handler
// ----------------------------------------------------------------------------

static const char* signal_name(int sig)
{
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        default:      return "UNKNOWN";
    }
}

static void crash_signal_handler(int sig, siginfo_t* info, void* context)
{
    // 1. Write breadcrumbs to stderr
    safe_write_str(STDERR_FILENO, "\n[CrashReporter] Caught signal: ");
    safe_write_str(STDERR_FILENO, signal_name(sig));
    safe_write_str(STDERR_FILENO, " (");
    safe_write_uint(STDERR_FILENO, (uint32_t)sig);
    safe_write_str(STDERR_FILENO, ")\n");

    if (info) {
        safe_write_str(STDERR_FILENO, "[CrashReporter] Fault address: 0x");
        // Write address as hex (signal-safe)
        {
            uintptr_t addr = (uintptr_t)info->si_addr;
            char hexbuf[20];
            int pos = 18;
            hexbuf[19] = '\0';
            hexbuf[pos] = '\0';
            if (addr == 0) {
                hexbuf[--pos] = '0';
            } else {
                while (addr > 0 && pos > 0) {
                    int nibble = addr & 0xF;
                    hexbuf[--pos] = nibble < 10 ? ('0' + nibble) : ('a' + nibble - 10);
                    addr >>= 4;
                }
            }
            safe_write_str(STDERR_FILENO, &hexbuf[pos]);
        }
        safe_write_str(STDERR_FILENO, "\n");
    }

    Rtt_BreadcrumbDump(STDERR_FILENO);

    // 2. Write to crash file if configured
    if (sCrashFilePath[0] != '\0') {
        int fd = open(sCrashFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            safe_write_str(fd, "Signal: ");
            safe_write_str(fd, signal_name(sig));
            safe_write_str(fd, "\n");
            Rtt_BreadcrumbDump(fd);
            close(fd);
        }
    }

#if defined(__ANDROID__) || defined(ANDROID)
    // 3. Write summary to logcat (async-signal-safe)
    __android_log_write(ANDROID_LOG_FATAL, "Corona",
        "CrashReporter: breadcrumbs dumped to stderr/file");
#endif

    // 4. Chain to previous handler or exit
    struct sigaction* old = NULL;
    switch (sig) {
        case SIGSEGV: old = &sOldSIGSEGV; break;
        case SIGABRT: old = &sOldSIGABRT; break;
        case SIGBUS:  old = &sOldSIGBUS;  break;
        case SIGILL:  old = &sOldSIGILL;  break;
        case SIGFPE:  old = &sOldSIGFPE;  break;
    }

    if (old && (old->sa_flags & SA_SIGINFO) && old->sa_sigaction) {
        old->sa_sigaction(sig, info, context);
    } else if (old && old->sa_handler != SIG_DFL && old->sa_handler != SIG_IGN) {
        old->sa_handler(sig);
    } else {
        // Reset to default and re-raise
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

// Alternate signal stack for stack-overflow SIGSEGV
// Note: sigaltstack is not available on tvOS/watchOS/emscripten
#if !defined( __TVOS_PROHIBITED ) && !defined( Rtt_TVOS_ENV ) && !defined( Rtt_EMSCRIPTEN_ENV )
static uint8_t sAltStack[SIGSTKSZ];
#endif

static void install_signal_handlers(void)
{
#if !defined( __TVOS_PROHIBITED ) && !defined( Rtt_TVOS_ENV ) && !defined( Rtt_EMSCRIPTEN_ENV )
    // Install alternate signal stack so we can handle stack-overflow SIGSEGV
    stack_t ss;
    ss.ss_sp = sAltStack;
    ss.ss_size = sizeof(sAltStack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);
#endif

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO;
#if !defined( __TVOS_PROHIBITED ) && !defined( Rtt_TVOS_ENV ) && !defined( Rtt_EMSCRIPTEN_ENV )
    sa.sa_flags |= SA_ONSTACK;
#endif
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, &sOldSIGSEGV);
    sigaction(SIGABRT, &sa, &sOldSIGABRT);
    sigaction(SIGBUS,  &sa, &sOldSIGBUS);
    sigaction(SIGILL,  &sa, &sOldSIGILL);
    sigaction(SIGFPE,  &sa, &sOldSIGFPE);
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void Rtt_CrashReporterInit(const char* crashFilePath)
{
    // Guard against double-initialization
    static bool sInitialized = false;
    if (sInitialized) return;
    sInitialized = true;

    // Pre-warm time function to avoid lazy-init in hot path
    (void)get_time_ms();

    // Store crash file path
    if (crashFilePath) {
        snprintf(sCrashFilePath, sizeof(sCrashFilePath), "%s", crashFilePath);
    }

    // Check for previous crash
    if (sCrashFilePath[0] != '\0') {
        struct stat st;
        if (stat(sCrashFilePath, &st) == 0 && st.st_size > 0) {
            // Previous crash detected - read and log it
            int fd = open(sCrashFilePath, O_RDONLY);
            if (fd >= 0) {
                char buf[4096];
                // Use fprintf here - we're NOT in a signal handler
                fprintf(stderr, "[CrashReporter] Previous crash detected:\n");
                ssize_t n;
                while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
                    buf[n] = '\0';
                    fprintf(stderr, "%s", buf);
                }
                close(fd);
                fprintf(stderr, "[CrashReporter] End of previous crash data.\n");
            }
            // Remove the crash file
            unlink(sCrashFilePath);
        }
    }

    // Install signal handlers
    install_signal_handlers();

    // Record init breadcrumb
    Rtt_BreadcrumbRecord(kBreadcrumb_Custom, "CrashReporter initialized");
}
