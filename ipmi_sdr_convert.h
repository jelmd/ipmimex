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
 * @file ipmi_sdr_units.h
 * SDR unit and unit convertion definitions/functions.
 */
#ifndef IPMIMEX_IPMI_SDR_CONVERT_H
#define IPMIMEX_IPMI_SDR_CONVERT_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert the given command completion code to a human readable string.
 * @param cc_code	The command completion code to convert.
 * @return \c NULL on error, a pointer to a static string otherwise.
 */
const char *ipmi_cc2str(uint8_t cc_code);

/**
 * @brief Convert a Sensor Unit Type Code (SDR byte 13) into a human readable
 *	string.
 * @param category	The code alias category to convert.
 * @return \c NULL on error, a pointer to a static string otherwise.
 * @see PMI v2, Table 42-3, Sensor Unit Type Codes. (42.2)
 */
const char *sdr_category2str(uint8_t code);

/**
 * @brief	Extract the sensor reading factors to use to compute the real sensor
 *	value from an SDR record.
 * @param sdr_factors	The SDR factors to convert.
 * @return \c NULL on error, a pointer to the allocated new \t factors
 *	otherwise. Use \c free() to deallocate it if not needed anymore.
 * @see	IPMI v2, table 43-1, byte 21 and 24..30 as well as 35.5.
 * @see \c sdr_convert_value().
 */
factors_t *sdr_factors2factors(sdr_factors_t *f);

/* @brief	Convert raw analog sensor reading.
 *
 * @param val	The value to convert.
 * @param afmt	The related Analog (numeric) Data Format to use. See 
 *	\c sdr_reading_t.unit.analog_fmt . If invalid (i.e. \c > \c 2) it gets
 *	returned as is.
 * @param factors	The reading factors of the sensor to be used for convertion.
 *	If \c NULL the given value gets returned as is.
 *
 * @return the converted value.
 *
 * @see	IPMI v2, 35.14, 
 */
double sdr_convert_value(uint8_t val, uint8_t afmt, factors_t *f);

/**
 * @brief Convert the Sensor 1, 2, and 3 properties of an SDR to a human
 *	readable string.
 * @param unit		A pointer to SDR sensor unit 1 (byte 21).
 *
 * @returns \c NULL on error, a pointer to static string which gets overwritten
 *	on the next call of this function otherwise.
 */
const char *sdr_unit2str(unit_t *unit);

/**
 * @brief	Convert a SDR name as specified in IPMI v2, table 43-1 to UTF-8.
 *
 * @param raw	A pointer to the raw SDR name to convert. No need to add '\0'.
 * @param len	The number of bytes the raw SDR name consists of.
 * @param fmt	The format of the raw SDR name. For now the following formats
 *	are supported:
 *		- 0 .. unicode
 *		- 1 .. BCD plus
 *		- 2 .. 6bit-ASCII, packed
 *		- 3 .. 8bit-latin1
 *
 * @return \c NULL if convertion failed, a pointer to the converted,
 *	'\0' terminated  name otherwise. The callee needs to make sure to \c free()
 *	the allocated string if it is not needed anymore.
 *
 * @see IPMI v2, table 43-1
 */
char *sdr_str2utf8(const uint8_t *raw, uint8_t len, uint8_t fmt);
 

#ifdef __cplusplus
}
#endif

#endif  // IPMIMEX_IPMI_SDR_CONVERT_H
