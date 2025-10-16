#ifndef FRANKLIN_CORE_COMPILER_MACROS_HPP
#define FRANKLIN_CORE_COMPILER_MACROS_HPP

#include <cstdio>
#include <cstdlib>

namespace franklin {

#ifndef FRANKLIN_CACHE_LINE_SIZE
#define FRANKLIN_CACHE_LINE_SIZE 64
#endif // FRANKLIN_CACHE_LINE_SIZE

#ifndef FRANKLIN_FORCE_INLINE
#define FRANKLIN_FORCE_INLINE __attribute__((always_inline))
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

#ifndef FRANKLIN_DEBUG_ASSERT
#ifdef NDEBUG
#define FRANKLIN_DEBUG_ASSERT(X) ((void)0)
#else
#define FRANKLIN_DEBUG_ASSERT(X) FRANKLIN_ASSERT(X)
#endif // NDEBUG
#endif // FRANKLIN_DEBUG_ASSERT

} // namespace franklin

#endif // FRANKLIN_CORE_COMPILER_MACROS_HPP