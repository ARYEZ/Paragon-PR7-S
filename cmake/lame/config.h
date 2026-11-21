/* (0.200.0) LAME's config.h for PR7-S's own build of libmp3lame (cmake/Dependencies.cmake,
   CMakeLists.txt). Written by hand in place of LAME's configure, for Linux, macOS and Windows
   (MSVC) alike: plain C, no assembly (no NASM, no SSE intrinsics: HAVE_XMMINTRIN_H and
   MIN_ARCH_SSE left out on purpose), single precision as LAME's own builds, no decoder. */
#ifndef PR7S_LAME_CONFIG_H
#define PR7S_LAME_CONFIG_H

#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define HAVE_STDINT_H 1      /* every compiler we build with has <stdint.h> (MSVC since 2010) */
#define PROTOTYPES 1
#define USE_FAST_LOG 1       /* as LAME's own Windows build (configMS.h) */
#define LAME_LIBRARY_BUILD 1
#define PACKAGE "lame"
#define VERSION "3.100"

#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#define SIZEOF_FLOAT 4
#define SIZEOF_DOUBLE 8
#define SIZEOF_UNSIGNED_SHORT 2
#define SIZEOF_UNSIGNED_INT 4

#include <stdint.h>

#define HAVE_IEEE754_FLOAT32_T 1
#define HAVE_IEEE754_FLOAT64_T 1
typedef float ieee754_float32_t;
typedef double ieee754_float64_t;
typedef long double ieee854_float80_t;

#if defined(_MSC_VER)
#  pragma warning (disable : 4244 4305 4267 4996 4018 4146)
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS 1
#  endif
#endif

#endif
