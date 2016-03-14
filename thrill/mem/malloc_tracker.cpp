/*******************************************************************************
 * thrill/mem/malloc_tracker.cpp
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2013-2016 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <thrill/mem/malloc_tracker.hpp>

#if __linux__ || __APPLE__ || __FreeBSD__

#include <dlfcn.h>

#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#if defined(__clang__) || defined (__GNUC__)

#define ATTRIBUTE_NO_SANITIZE_ADDRESS \
    __attribute__ ((no_sanitize_address)) /* NOLINT */

#if defined (__GNUC__) && __GNUC__ >= 5
#define ATTRIBUTE_NO_SANITIZE_THREAD \
    __attribute__ ((no_sanitize_thread))  /* NOLINT */
#else
#define ATTRIBUTE_NO_SANITIZE_THREAD
#endif

#define ATTRIBUTE_NO_SANITIZE \
    ATTRIBUTE_NO_SANITIZE_ADDRESS ATTRIBUTE_NO_SANITIZE_THREAD

#else
#define ATTRIBUTE_NO_SANITIZE
#endif

namespace thrill {
namespace mem {

/******************************************************************************/
// user-defined options for output malloc()/free() operations to stderr

static constexpr int log_operations = 0; //! <-- set this to 1 for log output
static constexpr size_t log_operations_threshold = 100000;

#define LOG_MALLOC_PROFILER 0

/******************************************************************************/
// variables of malloc tracker

//! In the generic hook implementation, we add to each allocation additional
//! data for bookkeeping.
static constexpr size_t padding = 16;    /* bytes (>= 2*sizeof(size_t)) */

//! function pointer to the real procedures, loaded using dlsym()
using malloc_type = void* (*)(size_t);
using free_type = void (*)(void*);
using realloc_type = void* (*)(void*, size_t);

static malloc_type real_malloc = nullptr;
static free_type real_free = nullptr;
static realloc_type real_realloc = nullptr;

//! a sentinel value prefixed to each allocation
static constexpr size_t sentinel = 0xDEADC0DE;

#define USE_ATOMICS 0

//! CounterType is used for atomic counters, and get to retrieve their contents
#if defined(_MSC_VER) || USE_ATOMICS
using CounterType = std::atomic<size_t>;
#else
// we cannot use std::atomic on gcc/clang because only real atomic instructions
// work with the Sanitizers
using CounterType = size_t;
#endif

// actually only such that formatting is not messed up
#define COUNTER_ZERO { 0 }

ATTRIBUTE_NO_SANITIZE
static inline size_t get(const CounterType& a) {
#if defined(_MSC_VER) || USE_ATOMICS
    return a.load();
#else
    return a;
#endif
}

ATTRIBUTE_NO_SANITIZE
static inline size_t sync_add_and_fetch(CounterType& curr, size_t inc) {
#if defined(_MSC_VER) || USE_ATOMICS
    return (curr += inc);
#else
    return __sync_add_and_fetch(&curr, inc);
#endif
}

ATTRIBUTE_NO_SANITIZE
static inline size_t sync_sub_and_fetch(CounterType& curr, size_t dec) {
#if defined(_MSC_VER) || USE_ATOMICS
    return (curr -= dec);
#else
    return __sync_sub_and_fetch(&curr, dec);
#endif
}

//! a simple memory heap for allocations prior to dlsym loading
#define INIT_HEAP_SIZE 1024 * 1024
static char init_heap[INIT_HEAP_SIZE];
static CounterType init_heap_use COUNTER_ZERO;
static constexpr int log_operations_init_heap = 0;

//! align allocations to init_heap to this number by rounding up allocations
static constexpr size_t init_alignment = sizeof(size_t);

//! output
#define PPREFIX "malloc_tracker ### "

/******************************************************************************/
// Run-time memory allocation statistics

static CounterType total_allocs COUNTER_ZERO;
static CounterType current_allocs COUNTER_ZERO;

static CounterType total_bytes COUNTER_ZERO;
static CounterType peak_bytes COUNTER_ZERO;

// free-floating memory allocated by malloc/free
static CounterType float_curr COUNTER_ZERO;

// Thrill base memory allocated by bypass_malloc/bypass_free
static CounterType base_curr COUNTER_ZERO;

//! memory limit exceeded indicator
bool memory_exceeded = false;
size_t memory_limit_indication = std::numeric_limits<size_t>::max();

// prototype for profiling
static void update_memprofile(
    size_t float_current, size_t base_current, bool flush = false);

void update_peak(size_t float_curr, size_t base_curr) {
    if (float_curr + base_curr > peak_bytes)
        peak_bytes = float_curr + base_curr;
}

//! add allocation to statistics
ATTRIBUTE_NO_SANITIZE
static void inc_count(size_t inc) {
    size_t mycurr = sync_add_and_fetch(float_curr, inc);

    total_bytes += inc;
    update_peak(mycurr, base_curr);

    sync_add_and_fetch(total_allocs, 1);
    sync_add_and_fetch(current_allocs, 1);

    memory_exceeded = (mycurr >= memory_limit_indication);
    update_memprofile(mycurr, get(base_curr));
}

//! decrement allocation to statistics
ATTRIBUTE_NO_SANITIZE
static void dec_count(size_t dec) {
    size_t mycurr = sync_sub_and_fetch(float_curr, dec);

    sync_sub_and_fetch(current_allocs, 1);

    memory_exceeded = (mycurr >= memory_limit_indication);
    update_memprofile(mycurr, get(base_curr));
}

//! user function to return the currently allocated amount of memory
size_t malloc_tracker_current() {
    return float_curr;
}

//! user function to return the peak allocation
size_t malloc_tracker_peak() {
    return peak_bytes;
}

//! user function to reset the peak allocation to current
void malloc_tracker_reset_peak() {
    peak_bytes = get(float_curr);
}

//! user function to return total number of allocations
size_t malloc_tracker_total_allocs() {
    return total_allocs;
}

//! user function which prints current and peak allocation to stderr
void malloc_tracker_print_status() {
    fprintf(stderr, PPREFIX "floating %zu, peak %zu, base %zu\n",
            get(float_curr), get(peak_bytes), get(base_curr));
}

void set_memory_limit_indication(size_t size) {
    // fprintf(stderr, PPREFIX "set_memory_limit_indication %zu\n", size);
    memory_limit_indication = size;
}

/******************************************************************************/
// Run-time memory profile writer

static bool enable_memprofile = false;

static FILE* memprofile_file = nullptr;

// default to 0.1 sec resolution
static long long memprofile_resolution = 100000;
static long long memprofile_last_update = 0;

struct OhlcBar {
    size_t open, high, low, close;

    void   init(size_t current) {
        open = high = low = close = current;
    }
    void   aggregate(size_t current) {
        high = std::max(high, current);
        low = std::min(low, current);
        close = current;
    }
};

// Two Ohlc bars: for free floating memory and for Thrill base memory.
static OhlcBar mp_float, mp_base;

static void init_memprofile() {

    // parse environment: THRILL_MEMPROFILE
    const char* env_pro = getenv("THRILL_MEMPROFILE");
    if (!env_pro) return;

    memprofile_file = fopen(env_pro, "wt");
    if (!memprofile_file) {
        fprintf(stderr, PPREFIX "could not open profile output: %s\n",
                env_pro);
        abort();
    }

    fprintf(memprofile_file, "ts epochmicro\n");
    fprintf(memprofile_file, "a skip\n");
    fprintf(memprofile_file, "float ohlc\n");
    fprintf(memprofile_file, "b skip\n");
    fprintf(memprofile_file, "base ohlc\n");
    fprintf(memprofile_file, "c skip\n");
    fprintf(memprofile_file, "sum ohlc\n");
    fprintf(memprofile_file, "\n");

    enable_memprofile = true;
}

static void update_memprofile(
    size_t float_current, size_t base_current, bool flush) {

    if (!enable_memprofile) return;

    using system_clock = std::chrono::system_clock;
    using duration = std::chrono::microseconds;

    long long now = std::chrono::duration_cast<duration>(
        system_clock::now().time_since_epoch()).count();

    now -= now % memprofile_resolution;

    if (now == memprofile_last_update && !flush)
    {
        // aggregate into OHLC bars
        mp_float.aggregate(float_current);
        mp_base.aggregate(base_current);
    }
    else
    {
        // output last bar
        if (memprofile_last_update != 0 || flush) {
            fprintf(memprofile_file,
                    "%lld float %zu %zu %zu %zu base %zu %zu %zu %zu "
                    "sum %zu %zu %zu %zu\n",
                    memprofile_last_update,
                    mp_float.open, mp_float.high, mp_float.low, mp_float.close,
                    mp_base.open, mp_base.high, mp_base.low, mp_base.close,
                    mp_float.open + mp_base.open,
                    mp_float.high + mp_base.high,
                    mp_float.low + mp_base.low,
                    mp_float.close + mp_base.close);
        }

        // start new OHLC bars
        memprofile_last_update = now;
        mp_float.init(float_current);
        mp_base.init(base_current);
    }
}

/******************************************************************************/
// Initialize function pointers to the real underlying malloc implementation.

#if __linux__ || __APPLE__ || __FreeBSD__

ATTRIBUTE_NO_SANITIZE
static __attribute__ ((constructor)) void init() { // NOLINT

    // try to use AddressSanitizer's malloc first.
    real_malloc = (malloc_type)dlsym(RTLD_DEFAULT, "__interceptor_malloc");
    if (real_malloc)
    {
        real_realloc = (realloc_type)dlsym(RTLD_DEFAULT, "__interceptor_realloc");
        if (!real_realloc) {
            fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
            exit(EXIT_FAILURE);
        }

        real_free = (free_type)dlsym(RTLD_DEFAULT, "__interceptor_free");
        if (!real_free) {
            fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, PPREFIX "using AddressSanitizer's malloc\n");

        init_memprofile();
        return;
    }

    real_malloc = (malloc_type)dlsym(RTLD_NEXT, "malloc");
    if (!real_malloc) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    real_realloc = (realloc_type)dlsym(RTLD_NEXT, "realloc");
    if (!real_realloc) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    real_free = (free_type)dlsym(RTLD_NEXT, "free");
    if (!real_free) {
        fprintf(stderr, PPREFIX "dlerror %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    init_memprofile();
}

ATTRIBUTE_NO_SANITIZE
static __attribute__ ((destructor)) void finish() { // NOLINT
    update_memprofile(get(float_curr), get(base_curr), /* flush */ true);
    fprintf(stderr, PPREFIX
            "exiting, total: %zu, peak: %zu, current: %zu / %zu, "
            "allocs: %zu, unfreed: %zu\n",
            get(total_bytes), get(peak_bytes),
            get(float_curr), get(base_curr),
            get(total_allocs), get(current_allocs));
}

#endif

/******************************************************************************/
// Functions to bypass the malloc tracker

//! bypass malloc tracker and access malloc() directly
ATTRIBUTE_NO_SANITIZE
void * bypass_malloc(size_t size) noexcept {
    size_t mycurr = sync_add_and_fetch(base_curr, size);

    total_bytes += size;
    update_peak(float_curr, mycurr);

    sync_add_and_fetch(total_allocs, 1);
    sync_add_and_fetch(current_allocs, 1);

    update_memprofile(get(float_curr), mycurr);

#if defined(_MSC_VER)
    return malloc(size);
#else
    return real_malloc(size);
#endif
}

//! bypass malloc tracker and access free() directly
ATTRIBUTE_NO_SANITIZE
void bypass_free(void* ptr, size_t size) noexcept {
    size_t mycurr = sync_sub_and_fetch(base_curr, size);

    sync_sub_and_fetch(current_allocs, 1);

    update_memprofile(get(float_curr), mycurr);

#if defined(_MSC_VER)
    return free(ptr);
#else
    return real_free(ptr);
#endif
}

} // namespace mem
} // namespace thrill

/******************************************************************************/
// exported symbols that overlay the libc functions

using namespace thrill::mem; // NOLINT

ATTRIBUTE_NO_SANITIZE
static void * preinit_malloc(size_t size) noexcept {

    size_t aligned_size = size + (init_alignment - size % init_alignment);

#if defined(_MSC_VER) || USE_ATOMICS
    size_t offset = (init_heap_use += (padding + aligned_size));
#else
    size_t offset = __sync_fetch_and_add(&init_heap_use, padding + aligned_size);
#endif

    if (offset > INIT_HEAP_SIZE) {
        fprintf(stderr, PPREFIX "init heap full !!!\n");
        exit(EXIT_FAILURE);
    }

    char* ret = init_heap + offset;

    //! prepend allocation size and check sentinel
    *reinterpret_cast<size_t*>(ret) = aligned_size;
    *reinterpret_cast<size_t*>(ret + padding - sizeof(size_t)) = sentinel;

    inc_count(aligned_size);

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "malloc(%zu / %zu) = %p   on init heap\n",
                size, aligned_size, static_cast<void*>(ret + padding));
    }

    return ret + padding;
}

ATTRIBUTE_NO_SANITIZE
static void * preinit_realloc(void* ptr, size_t size) {

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "realloc(%p) = on init heap\n", ptr);
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "realloc(%p) has no sentinel !!! memory corruption?\n",
                ptr);
    }

    size_t oldsize = *reinterpret_cast<size_t*>(ptr);

    if (oldsize >= size) {
        //! keep old area
        return static_cast<char*>(ptr) + padding;
    }
    else {
        //! allocate new area and copy data
        ptr = static_cast<char*>(ptr) + padding;
        void* newptr = malloc(size);
        memcpy(newptr, ptr, oldsize);
        free(ptr);
        return newptr;
    }
}

ATTRIBUTE_NO_SANITIZE
static void preinit_free(void* ptr) {
    // don't do any real deallocation.

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n",
                ptr);
    }

    size_t size = *reinterpret_cast<size_t*>(ptr);
    dec_count(size);

    if (log_operations_init_heap) {
        fprintf(stderr, PPREFIX "free(%p) -> %zu   on init heap\n", ptr, size);
    }
}

#if __APPLE__

#define NOEXCEPT
#define MALLOC_USABLE_SIZE malloc_size
#include <malloc/malloc.h>

#elif __FreeBSD__

#define NOEXCEPT
#define MALLOC_USABLE_SIZE malloc_usable_size
#include <malloc_np.h>

#elif __linux__

#define NOEXCEPT noexcept
#define MALLOC_USABLE_SIZE malloc_usable_size
#include <malloc.h>

#include <execinfo.h>
#include <unistd.h>

#endif

/******************************************************************************/

#if defined(MALLOC_USABLE_SIZE)

/*
 * This is a malloc() tracker implementation which uses an available system call
 * to determine the amount of memory used by an allocation (which may be more
 * than the allocated size). On Linux's glibc there is malloc_usable_size().
 */

//! exported malloc symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * malloc(size_t size) NOEXCEPT {

    if (!real_malloc)
        return preinit_malloc(size);

    //! call real malloc procedure in libc
    void* ret = (*real_malloc)(size);
    if (!ret) return nullptr;

    size_t size_used = MALLOC_USABLE_SIZE(ret);
    inc_count(size_used);

    if (log_operations && size_used >= log_operations_threshold) {
        fprintf(stderr, PPREFIX "malloc(%zu size / %zu used) = %p   (current %zu / %zu)\n",
                size, size_used, ret, get(float_curr), get(base_curr));
    }
    {
#if __linux__ && LOG_MALLOC_PROFILER
        static thread_local bool recursive = false;

        if (size_used >= log_operations_threshold && !recursive) {
            recursive = true;

/*[[[perl
  # depth of call stack to print
  my $depth = 16;

  print <<EOF;
            // storage array for stack trace address data
            void* addrlist[$depth + 1];

            // retrieve current stack addresses
            int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

EOF

  for my $d (3...$depth-1) {
    print("fprintf(stdout, PPREFIX \"caller".(" %p"x$d)." size %zu\\n\",\n");
    print("        ".join("", map("addrlist[$_], ", 0...$d-1))." size);");
  }
  ]]]*/
            // storage array for stack trace address data
            void* addrlist[16 + 1];

            // retrieve current stack addresses
            int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

            fprintf(stdout, PPREFIX "caller %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], addrlist[10], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], addrlist[10], addrlist[11], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], addrlist[10], addrlist[11], addrlist[12], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], addrlist[10], addrlist[11], addrlist[12], addrlist[13], size);
            fprintf(stdout, PPREFIX "caller %p %p %p %p %p %p %p %p %p %p %p %p %p %p %p size %zu\n",
                    addrlist[0], addrlist[1], addrlist[2], addrlist[3], addrlist[4], addrlist[5], addrlist[6], addrlist[7], addrlist[8], addrlist[9], addrlist[10], addrlist[11], addrlist[12], addrlist[13], addrlist[14], size);
// [[[end]]]

            // fprintf(stderr, "--- begin stack -------------------------------\n");
            // backtrace_symbols_fd(addrlist, addrlen, STDERR_FILENO);
            // fprintf(stderr, "--- end stack ---------------------------------\n");

            recursive = false;
        }
#endif
    }

    return ret;
}

//! exported free symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void free(void* ptr) NOEXCEPT {

    if (!ptr) return;   //! free(nullptr) is no operation

    if (static_cast<char*>(ptr) >= init_heap &&
        static_cast<char*>(ptr) <= init_heap + get(init_heap_use))
    {
        return preinit_free(ptr);
    }

    if (!real_free) {
        fprintf(stderr, PPREFIX
                "free(%p) outside init heap and without real_free !!!\n", ptr);
        return;
    }

    size_t size_used = MALLOC_USABLE_SIZE(ptr);
    dec_count(size_used);

    if (log_operations && size_used >= log_operations_threshold) {
        fprintf(stderr, PPREFIX "free(%p) -> %zu   (current %zu / %zu)\n",
                ptr, size_used, get(float_curr), get(base_curr));
    }

    (*real_free)(ptr);
}

//! exported calloc() symbol that overrides loading from libc, implemented using
//! our malloc
ATTRIBUTE_NO_SANITIZE
void * calloc(size_t nmemb, size_t size) NOEXCEPT {
    size *= nmemb;
    void* ret = malloc(size);
    if (!ret) return ret;
    memset(ret, 0, size);
    return ret;
}

//! exported realloc() symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * realloc(void* ptr, size_t size) NOEXCEPT {

    if (static_cast<char*>(ptr) >= static_cast<char*>(init_heap) &&
        static_cast<char*>(ptr) <= static_cast<char*>(init_heap) + get(init_heap_use))
    {
        return preinit_realloc(ptr, size);
    }

    if (size == 0) { //! special case size == 0 -> free()
        free(ptr);
        return nullptr;
    }

    if (ptr == nullptr) { //! special case ptr == 0 -> malloc()
        return malloc(size);
    }

    size_t oldsize_used = MALLOC_USABLE_SIZE(ptr);
    dec_count(oldsize_used);

    void* newptr = (*real_realloc)(ptr, size);
    if (!newptr) return nullptr;

    size_t newsize_used = MALLOC_USABLE_SIZE(newptr);
    inc_count(newsize_used);

    if (log_operations && newsize_used >= log_operations_threshold)
    {
        if (newptr == ptr) {
            fprintf(stderr, PPREFIX
                    "realloc(%zu -> %zu / %zu) = %p   (current %zu / %zu)\n",
                    oldsize_used, size, newsize_used, newptr,
                    get(float_curr), get(base_curr));
        }
        else {
            fprintf(stderr, PPREFIX
                    "realloc(%zu -> %zu / %zu) = %p -> %p   (current %zu / %zu)\n",
                    oldsize_used, size, newsize_used, ptr, newptr,
                    get(float_curr), get(base_curr));
        }
    }

    return newptr;
}

/******************************************************************************/

#elif !defined(_MSC_VER) // GENERIC IMPLEMENTATION for Unix

/*
 * This is a generic implementation to count memory allocation by prefixing
 * every user allocation with the size. On free, the size can be
 * retrieves. Obviously, this wastes lots of memory if there are many small
 * allocations.
 */

//! exported malloc symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * malloc(size_t size) NOEXCEPT {

    if (!real_malloc)
        return preinit_malloc(size);

    //! call real malloc procedure in libc
    void* ret = (*real_malloc)(padding + size);

    inc_count(size);
    if (log_operations && size >= log_operations_threshold) {
        fprintf(stderr, PPREFIX "malloc(%zu) = %p   (current %zu / %zu)\n",
                size, static_cast<char*>(ret) + padding,
                get(float_curr), get(base_curr));
    }

    //! prepend allocation size and check sentinel
    *reinterpret_cast<size_t*>(ret) = size;
    *reinterpret_cast<size_t*>(
        static_cast<char*>(ret) + padding - sizeof(size_t)) = sentinel;

    return static_cast<char*>(ret) + padding;
}

//! exported free symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void free(void* ptr) NOEXCEPT {

    if (!ptr) return;   //! free(nullptr) is no operation

    if (static_cast<char*>(ptr) >= init_heap &&
        static_cast<char*>(ptr) <= init_heap + get(init_heap_use))
    {
        return preinit_free(ptr);
    }

    if (!real_free) {
        fprintf(stderr, PPREFIX
                "free(%p) outside init heap and without real_free !!!\n", ptr);
        return;
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    size_t size = *reinterpret_cast<size_t*>(ptr);
    dec_count(size);

    if (log_operations && size >= log_operations_threshold) {
        fprintf(stderr, PPREFIX "free(%p) -> %zu   (current %zu / %zu)\n",
                ptr, size, get(float_curr), get(base_curr));
    }

    (*real_free)(ptr);
}

//! exported calloc() symbol that overrides loading from libc, implemented using
//! our malloc
ATTRIBUTE_NO_SANITIZE
void * calloc(size_t nmemb, size_t size) NOEXCEPT {
    size *= nmemb;
    if (!size) return nullptr;
    void* ret = malloc(size);
    memset(ret, 0, size);
    return ret;
}

//! exported realloc() symbol that overrides loading from libc
ATTRIBUTE_NO_SANITIZE
void * realloc(void* ptr, size_t size) NOEXCEPT {

    if (static_cast<char*>(ptr) >= static_cast<char*>(init_heap) &&
        static_cast<char*>(ptr) <=
        static_cast<char*>(init_heap) + get(init_heap_use))
    {
        return preinit_realloc(ptr, size);
    }

    if (size == 0) { //! special case size == 0 -> free()
        free(ptr);
        return nullptr;
    }

    if (ptr == nullptr) { //! special case ptr == 0 -> malloc()
        return malloc(size);
    }

    ptr = static_cast<char*>(ptr) - padding;

    if (*reinterpret_cast<size_t*>(
            static_cast<char*>(ptr) + padding - sizeof(size_t)) != sentinel) {
        fprintf(stderr, PPREFIX
                "free(%p) has no sentinel !!! memory corruption?\n", ptr);
    }

    size_t oldsize = *reinterpret_cast<size_t*>(ptr);

    dec_count(oldsize);
    inc_count(size);

    void* newptr = (*real_realloc)(ptr, padding + size);

    if (log_operations && size >= log_operations_threshold)
    {
        if (newptr == ptr)
            fprintf(stderr, PPREFIX
                    "realloc(%zu -> %zu) = %p   (current %zu / %zu)\n",
                    oldsize, size, newptr, get(float_curr), get(base_curr));
        else
            fprintf(stderr, PPREFIX
                    "realloc(%zu -> %zu) = %p -> %p   (current %zu / %zu)\n",
                    oldsize, size, ptr, newptr, get(float_curr), get(base_curr));
    }

    *reinterpret_cast<size_t*>(newptr) = size;

    return static_cast<char*>(newptr) + padding;
}

/******************************************************************************/

#else // if defined(_MSC_VER)

// TODO(tb): dont know how to override malloc/free.

#endif // IMPLEMENTATION SWITCH

/******************************************************************************/
