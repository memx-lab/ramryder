#ifndef UTIL_COMMON_H
#define UTIL_COMMON_H
#include <unistd.h>
#include <signal.h>
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
	#define likely(x)   __builtin_expect(!!(x), 1)
	#define unlikely(x) __builtin_expect(!!(x), 0)
#else
	#define likely(x)   (x)
	#define unlikely(x) (x)
#endif

#define BUG_ON(cond)											\
	do {												\
		if (cond) {										\
			fprintf(stderr, "BUG_ON: %s (L%d) %s\n", __FILE__, __LINE__, __FUNCTION__);	\
			raise(SIGABRT);									\
		}											\
	} while (0)

#ifdef static_assert
	#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
	#define STATIC_ASSERT(cond, msg)
#endif

// counter related
#define US_PER_SECOND 1000000
#define US_PER_MS 1000
#define SECOND_TO_US(x) ((x) * (US_PER_SECOND))
#define US_TO_SECOND(x) ((x) / (US_PER_SECOND))

#define BYTES_PER_KB 1024UL
#define BYTES_PER_MB (1024UL * 1024UL)
#define BYTES_PER_GB (1024UL * 1024UL * 1024UL)
#define KB_TO_BYTES(x) ((size_t)(x) * (size_t)(BYTES_PER_KB))
#define MB_TO_BYTES(x) ((size_t)(x) * (size_t)(BYTES_PER_MB))
#define GB_TO_BYTES(x) ((size_t)(x) * (size_t)(BYTES_PER_GB))

#define MBYTES_PER_GB 1024UL
#define MB_TO_GB(x) ((size_t)(x) / (size_t)(MBYTES_PER_GB))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif // UTIL_COMMON_H