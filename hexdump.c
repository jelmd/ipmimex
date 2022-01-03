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

#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>

#include <prom_log.h>

#define BPL 77		/* we consume 77 byte/line  => 77 byte /16 byte chunk */

size_t
bdump(const uint8_t *data, size_t dlen, char *buf, size_t buflen, int hex) {
	const uint8_t *q, *dend;
	const char *fmt, *fmt_head;
	char *p, *e;
	size_t d, max, i;
	uint8_t c;
	size_t offset = (hex == 0 || hex == 2) ? 14 : 0;

	const char H2C[] = "0123456789abcdef";

	if (hex) {
		fmt = "%8X: ";
		fmt_head = "          00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F";
	} else {
		fmt = "%8ld: ";
		fmt_head = "          00 01 02 03 04 05 06 07  08 09 19 11 12 13 14 15";
	}

	max = (buflen - strlen(fmt_head) - 2 - 1) / BPL * 16 - offset;/* "\n\n\0" */
	if (max == 0) {
		PROM_WARN("Dump buffer too small. Expecting at least %ld",
			strlen(fmt_head) + 3 + BPL);
		return 0;
	}

	if (dlen > max) {
		PROM_WARN("Dump buffer too small. Dumping %ld of %ld bytes", max, dlen);
		dlen = max;
	}
	/* No one wants to dump 16 MiB+ */
	if (dlen > (0xFFFFFF - offset)) {
		PROM_WARN("Reducing dump size to 16 MiB", "");
		dlen = 0xFFFFFF - offset;
	}
	dend = data + dlen;

	snprintf(buf, buflen, "%s\n\n", fmt_head);
	max = strlen(buf);
	p = buf + max;
	e = p + (BPL - 16 - 1);				/* offset for char decoding */
	q = data;
	max = offset;
	for (d = 0; d < (dlen + offset); d += 16) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
		p += sprintf(p, fmt, d);
#pragma GCC diagnostic pop
		for (i = 0; i < 16 && q < dend; i++) {
			if (max != 0) {
				*p++ = ' '; *p++ = ' '; *e++ = ' ';
				max--;
			} else {
				c =	*q++;
				if (c < 15) {
					*p++ = '0';
					*p++ = H2C[c];
					*e++ = '.';
				} else {
					*p++ = H2C[(c >> 4) & 0x0F];
					*p++ = H2C[c & 0x0F];
					// UTF-8 terminals print a wc for 127, so we skip it
					*e++ =  (c < 32 || c > 126) ? '.' : c;
				}
			}
			*p++ = ' ';
			if (i == 7)
				*p++ = ' ';
		}
		if (i == 16) {
			*p++ = ' ';
			*e++ = '\n';
			p += 16 + 1;
			e += (BPL - 16 - 1);
		}
	}
	/* finish the line (may contain garbage) */
	max = (dlen + offset) & 0xF;
	if (max != 0) {
		if (max < 8 )
			*p++ = ' ';
		for (i = max; i < 16; i++) {
			*p++ = ' '; *p++ = ' '; *p++ = ' '; *e++ = ' ';
		}
		*p++ = ' ';
		*e++ = '\n';
	} else {
		e -= (BPL - 16 - 1);
	}
	*e = '\0';
	return e - buf;
}

#define HEX_DUMP_BUF_SZ 64*1024
char *
hexdump(const uint8_t *data, size_t dlen, int hex) {
	static char buf[HEX_DUMP_BUF_SZ];
	bdump(data, dlen, buf, HEX_DUMP_BUF_SZ, hex);
	return buf;
}

