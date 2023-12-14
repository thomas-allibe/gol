#ifndef _TYPES_H_
#define _TYPES_H_

#include "stdint.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

// Since scientific notation returns float, define the following for integers:
// TENPOW_3 is kinda overkill though
#define TENPOW_3 1000
#define TENPOW_4 10000
#define TENPOW_5 100000
#define TENPOW_6 1000000
#define TENPOW_7 10000000
#define TENPOW_8 100000000
#define TENPOW_9 1000000000
#define TENPOW_10 10000000000
#define TENPOW_11 100000000000
#define TENPOW_12 1000000000000

#endif // !_TYPES_H_
