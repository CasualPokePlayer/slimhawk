#ifndef __MDFN_SLIM_TYPES_H
#define __MDFN_SLIM_TYPES_H

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define MDFN_ASSUME_ALIGNED(p, align) __builtin_assume_aligned((p), (align))

#define MDFN_WARN_UNUSED_RESULT __attribute__ ((warn_unused_result))

#define MDFN_COLD __attribute__((cold))

#define NO_INLINE __attribute__((noinline))

#define MDFN_UNLIKELY(n) __builtin_expect((n) != 0, 0)
#define MDFN_LIKELY(n) __builtin_expect((n) != 0, 1)

#define MDFN_NOWARN_UNUSED __attribute__((unused))

#define MDFN_FORMATSTR(a,b,c) __attribute__((format (a, b, c)))

#define INLINE inline __attribute__((always_inline))

#ifndef FALSE
 #define FALSE 0
#endif

#ifndef TRUE
 #define TRUE 1
#endif

#define SIZEOF_DOUBLE 8

#define LSB_FIRST

#define EXPORT extern "C" __attribute__((visibility("default")))

#endif
