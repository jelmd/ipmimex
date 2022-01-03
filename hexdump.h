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
 * @file hexdump.h
 * Diagnostic utils.
 */

#ifndef IPMIMEX_HEXDUMP_H
#define IPMIMEX_HEXDUMP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hex dump the @data in chunks of 16 bytes.
 * @param data    Data to dump.
 * @param dlen    Number of bytes to dump.
 * @param buf     Where to store the dump.
 * @param buflen  Size of the @buf. If the size is \c < @dlen, remaining bytes
 *		are dropped.
 * @param hex     If \c 0, print offset in decimal, otherwise in hex.
 * @return The number of characters written excluding the terminating '\0'.
 */
size_t bdump(const uint8_t *data, size_t dlen, char *buf, size_t buflen, int hex);

/**
 * @brief  Same as \c bdump() but uses an internal static 64 KiB buffer to
 *	store the result. The next call of this function overwrites the data in the
 *	buffer, so not thread-safe.
 * @param data    Data to dump.
 * @param dlen    Number of bytes to dump.
 * @param hex     If 0, print offset in decimal, otherwise in hex.
 * @return A pointer to the static internal '\0' terminated result buffer.
 */
char *hexdump(const uint8_t *data, size_t dlen, int hex);

#ifdef __cplusplus
}
#endif

#endif	// IPMIMEX_HEXDUMP_H
