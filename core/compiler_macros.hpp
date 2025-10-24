#ifndef FRANKLIN_CORE_COMPILER_MACROS_HPP
#define FRANKLIN_CORE_COMPILER_MACROS_HPP

#include <cstdio>
#include <cstdlib>
#include <execinfo.h>

namespace franklin {

#ifndef FRANKLIN_CACHE_LINE_SIZE
#define FRANKLIN_CACHE_LINE_SIZE 64
#endif // FRANKLIN_CACHE_LINE_SIZE

#ifndef FRANKLIN_FORCE_INLINE
#define FRANKLIN_FORCE_INLINE __attribute__((always_inline)) inline
#endif // FRANKLIN_FORCE_INLINE

#ifndef FRANKLIN_FORCE_NOINLINE
#define FRANKLIN_FORCE_NOINLINE __attribute__((noinline))
#endif // FRANKLIN_FORCE_INLINE

#ifndef FRANKLIN_ASSERT
#define FRANKLIN_ASSERT(X)                                                     \
  do {                                                                         \
    if (!(X)) {                                                                \
      std::fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", #X,     \
                   __FILE__, __LINE__);                                        \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
#endif // FRANKLIN_ASSERT

#ifndef FRANKLIN_ASSERT_MSG
#define FRANKLIN_ASSERT_MSG(COND, MSG)                                         \
  do {                                                                         \
    if (!(COND)) {                                                             \
      std::fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", (MSG),  \
                   __FILE__, __LINE__);                                        \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
#endif // FRANKLIN_ASSERT_MSG

#ifndef FRANKLIN_DEBUG_ASSERT
#ifdef NDEBUG
#define FRANKLIN_DEBUG_ASSERT(X) ((void)0)
#else
#define FRANKLIN_DEBUG_ASSERT(X) FRANKLIN_ASSERT(X)
#endif // NDEBUG
#endif // FRANKLIN_DEBUG_ASSERT

#ifndef FRANKLIN_ABORT
#define FRANKLIN_ABORT(msg)                                                    \
  do {                                                                         \
    std::fprintf(stderr, "Fatal error: %s, file %s, line %d\n", (msg),         \
                 __FILE__, __LINE__);                                          \
    std::fprintf(stderr, "Stack trace:\n");                                    \
    void* callstack[128];                                                      \
    int frames = backtrace(callstack, 128);                                    \
    char** symbols = backtrace_symbols(callstack, frames);                     \
    for (int i = 0; i < frames; ++i) {                                         \
      std::fprintf(stderr, "  %s\n", symbols[i]);                              \
    }                                                                          \
    free(symbols);                                                             \
    std::abort();                                                              \
  } while (0)
#endif // FRANKLIN_ABORT

} // namespace franklin

#endif // FRANKLIN_CORE_COMPILER_MACROS_HPP