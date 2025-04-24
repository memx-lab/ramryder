#ifndef UTIL_COMMON_H
#define UTIL_COMMON_H
#include <unistd.h>
#include <signal.h>

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
#define SECOND_IN_US 1000000
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif // UTIL_COMMON_H