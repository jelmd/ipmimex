/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License") 1.1!
 * You may not use this file except in compliance with the License.
 *
 * See  https://spdx.org/licenses/CDDL-1.1.html  for the specific
 * language governing permissions and limitations under the License.
 *
 * Copyright 2022 Jens Elkner (jel+ipmimex-src@cs.ovgu.de)
 */

/**
 * @file mach.h
 * Generic and OS related macros, types, definitions.
 */
#ifndef IPMIMEX_MACH_H
#define IPMIMEX_MACH_H

#include <sys/sysmacros.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PACKED __attribute__ ((packed))

#ifdef __linux
	#include <endian.h>
	#include <byteswap.h>

	#define BSWAP_16(x) bswap_16(x)
	#define BSWAP_32(x) bswap_32(x)
#else
	#include <sys/byteorder.h>
#endif

#ifndef ARRAY_SIZE
#define	ARRAY_SIZE(x)	(sizeof (x) / sizeof (x[0]))
#endif

#ifndef __BYTE_ORDER
#ifdef __BYTE_ORDER__
#define __BYTE_ORDER __BYTE_ORDER__
#endif

#endif
#ifndef __BIG_ENDIAN
#ifdef _BIG_ENDIAN
#define __BIG_ENDIAN __BYTE_ORDER
#define __LITTLE_ENDIAN 0
#endif
#endif
#ifndef __LITTLE_ENDIAN
#ifdef _LITTLE_ENDIAN
#define __LITTLE_ENDIAN __BYTE_ORDER
#define __BIG_ENDIAN 0
#endif
#endif

/*
 * Macros to declare bitfields - the order in the parameter list is High to Low,
 * i.e. bit 7 first.
 */
#if __BYTE_ORDER == __BIG_ENDIAN
#define BITFIELD2(_a, _b)							\
    uint8_t _a, _b
#define BITFIELD3(_a, _b, _c)						\
    uint8_t _a, _b, _c
#define BITFIELD4(_a, _b, _c, _d)					\
    uint8_t _a, _b, _c, _d
#define BITFIELD5(_a, _b, _c, _d, _e)				\
    uint8_t _a, _b, _c, _d, _e
#define BITFIELD6(_a, _b, _c, _d, _e, _f)			\
    uint8_t _a, _b, _c, _d, _e, _f
#define BITFIELD7(_a, _b, _c, _d, _e, _f, _g)		\
    uint8_t _a, _b, _c, _d, _e, _f, _g
#define BITFIELD8(_a, _b, _c, _d, _e, _f, _g, _h)	\
    uint8_t _a, _b, _c, _d, _e, _f, _g, _h
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define BITFIELD2(_a, _b)							\
    uint8_t _b, _a
#define BITFIELD3(_a, _b, _c)						\
    uint8_t _c, _b, _a
#define BITFIELD4(_a, _b, _c, _d)					\
    uint8_t _d, _c, _b, _a
#define BITFIELD5(_a, _b, _c, _d, _e)				\
    uint8_t _e, _d, _c, _b, _a
#define BITFIELD6(_a, _b, _c, _d, _e, _f)			\
    uint8_t _f, _e, _d, _c, _b, _a
#define BITFIELD7(_a, _b, _c, _d, _e, _f, _g)		\
    uint8_t _g, _f, _e, _d, _c, _b, _a
#define BITFIELD8(_a, _b, _c, _d, _e, _f, _g, _h)	\
    uint8_t _h, _g, _f, _e, _d, _c, _b, _a
#else
#error  __BYTE_ORDER should be __BIG_ENDIAN or __LITTLE_ENDIAN
#endif  // bitfields


#ifdef __cplusplus
}
#endif

#endif  // IPMIMEX_MACH_H
