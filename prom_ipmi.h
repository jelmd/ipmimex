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
 * @file prom_ipmi.h
 * Prom related defintions/functions/etc.
 */
#ifndef IPMIMEX_PROM_IPMI_H
#define IPMIMEX_PROM_IPMI_H

#ifdef __cplusplus
extern "C" {
#endif

void collect_ipmi(psb_t *sb, sensor_t *slist);
void collect_dcmi(psb_t *sb, bool compact, bool sample);

/**
 * @brief Convert a Sensor Unit Type Code (SDR byte 13) into a human readable
 *	string.
 * @param category	The code alias category to convert.
 * @return	\c NULL on error, a pointer to a static string otherwise.
 * @see	PMI v2, Table 42-3, Sensor Unit Type Codes. (42.2)
 */
const char *category2prom(uint8_t code);

/**
 * @brief Convert the Sensor 1, 2, and 3 properties of an SDR to a prom string.
 * @param unit		A pointer to SDR sensor unit 1 (byte 21).
 *
 * @returns	\c NULL on error, a pointer to static string which gets overwritten
 *	on the next call of this function otherwise.
 */
const char *unit2prom(unit_t *u);

#ifdef __cplusplus
}
#endif

#endif  // IPMIMEX_PROM_IPMI_H
