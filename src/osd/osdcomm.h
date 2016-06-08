// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    osdcomm.h

    Common definitions shared by the OSD layer. This includes the most
    fundamental integral types as well as compiler-specific tweaks.

***************************************************************************/

#pragma once

#ifndef MAME_OSD_OSDCOMM_H
#define MAME_OSD_OSDCOMM_H

#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <type_traits>


/***************************************************************************
    COMPILER-SPECIFIC NASTINESS
***************************************************************************/

/* The Win32 port requires this constant for variable arg routines. */
#ifndef CLIB_DECL
#define CLIB_DECL
#endif


/* Some optimizations/warnings cleanups for GCC */
#if defined(__GNUC__)
#define ATTR_UNUSED             __attribute__((__unused__))
#define ATTR_NORETURN           __attribute__((noreturn))
#define ATTR_PRINTF(x,y)        __attribute__((format(printf, x, y)))
#define ATTR_CONST              __attribute__((const))
#define ATTR_FORCE_INLINE       __attribute__((always_inline))
#define ATTR_NONNULL(...)       __attribute__((nonnull(__VA_ARGS__)))
#define ATTR_DEPRECATED         __attribute__((deprecated))
#define ATTR_HOT                __attribute__((hot))
#define ATTR_COLD               __attribute__((cold))
#define UNEXPECTED(exp)         __builtin_expect(!!(exp), 0)
#define EXPECTED(exp)           __builtin_expect(!!(exp), 1)
#define RESTRICT                __restrict__
#else
#define ATTR_UNUSED
#define ATTR_NORETURN           __declspec(noreturn)
#define ATTR_PRINTF(x,y)
#define ATTR_CONST
#define ATTR_FORCE_INLINE       __forceinline
#define ATTR_NONNULL(...)
#define ATTR_DEPRECATED         __declspec(deprecated)
#define ATTR_HOT
#define ATTR_COLD
#define UNEXPECTED(exp)         (exp)
#define EXPECTED(exp)           (exp)
#define RESTRICT
#endif



/***************************************************************************
    FUNDAMENTAL TYPES
***************************************************************************/


/* 8-bit values */
using UINT8 = std::uint8_t;
using INT8 = std::int8_t;

/* 16-bit values */
using UINT16 = std::uint16_t;
using INT16 = std::int16_t;

/* 32-bit values */
using UINT32 = std::uint32_t;
using INT32 = std::int32_t;

/* 64-bit values */
using UINT64 = std::uint64_t;
using INT64 = std::int64_t;

/* pointer-sized values */
using FPTR = uintptr_t;



/***************************************************************************
    FUNDAMENTAL CONSTANTS
***************************************************************************/

/* Ensure that TRUE/FALSE are defined */
#ifndef TRUE
#define TRUE                1
#endif

#ifndef FALSE
#define FALSE               0
#endif



/***************************************************************************
    FUNDAMENTAL MACROS
***************************************************************************/

/* Standard MIN/MAX macros */
#ifndef MIN
#define MIN(x,y)            ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y)            ((x) > (y) ? (x) : (y))
#endif


/* U64 and S64 are used to wrap long integer constants. */
#if defined(__GNUC__) || defined(_MSC_VER)
#define U64(val) val##ULL
#define S64(val) val##LL
#else
#define U64(val) val
#define S64(val) val
#endif


/* Concatenate/extract 32-bit halves of 64-bit values */
#define CONCAT_64(hi,lo)    (((UINT64)(hi) << 32) | (UINT32)(lo))
#define EXTRACT_64HI(val)   ((UINT32)((val) >> 32))
#define EXTRACT_64LO(val)   ((UINT32)(val))

// Highly useful template for compile-time knowledge of an array size
template <typename T, size_t N> constexpr inline size_t ARRAY_LENGTH(T (&)[N]) { return N;}

// For declaring an array of the same dimensions as another array (including multi-dimensional arrays)
template <typename T, typename U> struct equivalent_array_or_type { typedef T type; };
template <typename T, typename U, std::size_t N> struct equivalent_array_or_type<T, U[N]> { typedef typename equivalent_array_or_type<T, U>::type type[N]; };
template <typename T, typename U> using equivalent_array_or_type_t = typename equivalent_array_or_type<T, U>::type;
template <typename T, typename U> struct equivalent_array { };
template <typename T, typename U, std::size_t N> struct equivalent_array<T, U[N]> { typedef equivalent_array_or_type_t<T, U> type[N]; };
template <typename T, typename U> using equivalent_array_t = typename equivalent_array<T, U>::type;
#define EQUIVALENT_ARRAY(a, T) equivalent_array_t<T, std::remove_reference_t<decltype(a)> >


/* Macros for normalizing data into big or little endian formats */
#define FLIPENDIAN_INT16(x) (((((UINT16) (x)) >> 8) | ((x) << 8)) & 0xffff)
#define FLIPENDIAN_INT32(x) ((((UINT32) (x)) << 24) | (((UINT32) (x)) >> 24) | \
	(( ((UINT32) (x)) & 0x0000ff00) << 8) | (( ((UINT32) (x)) & 0x00ff0000) >> 8))
#define FLIPENDIAN_INT64(x) \
	(                                               \
		(((((UINT64) (x)) >> 56) & ((UINT64) 0xFF)) <<  0)  |   \
		(((((UINT64) (x)) >> 48) & ((UINT64) 0xFF)) <<  8)  |   \
		(((((UINT64) (x)) >> 40) & ((UINT64) 0xFF)) << 16)  |   \
		(((((UINT64) (x)) >> 32) & ((UINT64) 0xFF)) << 24)  |   \
		(((((UINT64) (x)) >> 24) & ((UINT64) 0xFF)) << 32)  |   \
		(((((UINT64) (x)) >> 16) & ((UINT64) 0xFF)) << 40)  |   \
		(((((UINT64) (x)) >>  8) & ((UINT64) 0xFF)) << 48)  |   \
		(((((UINT64) (x)) >>  0) & ((UINT64) 0xFF)) << 56)      \
	)

#ifdef LSB_FIRST
#define BIG_ENDIANIZE_INT16(x)      (FLIPENDIAN_INT16(x))
#define BIG_ENDIANIZE_INT32(x)      (FLIPENDIAN_INT32(x))
#define BIG_ENDIANIZE_INT64(x)      (FLIPENDIAN_INT64(x))
#define LITTLE_ENDIANIZE_INT16(x)   (x)
#define LITTLE_ENDIANIZE_INT32(x)   (x)
#define LITTLE_ENDIANIZE_INT64(x)   (x)
#else
#define BIG_ENDIANIZE_INT16(x)      (x)
#define BIG_ENDIANIZE_INT32(x)      (x)
#define BIG_ENDIANIZE_INT64(x)      (x)
#define LITTLE_ENDIANIZE_INT16(x)   (FLIPENDIAN_INT16(x))
#define LITTLE_ENDIANIZE_INT32(x)   (FLIPENDIAN_INT32(x))
#define LITTLE_ENDIANIZE_INT64(x)   (FLIPENDIAN_INT64(x))
#endif /* LSB_FIRST */

#ifdef _MSC_VER
#include <malloc.h>
typedef ptrdiff_t ssize_t;
#if _MSC_VER == 1900 // VS2015
#define __LINE__Var 0
#endif // VS2015
#if _MSC_VER < 1900 // VS2013 or earlier
#define snprintf _snprintf
#define __func__ __FUNCTION__
#else // VS2015
#define _CRT_STDIO_LEGACY_WIDE_SPECIFIERS
#endif
#endif

#ifdef __GNUC__
#ifndef alloca
#define alloca(size)  __builtin_alloca(size)
#endif
#endif

#endif  /* MAME_OSD_OSDCOMM_H */
