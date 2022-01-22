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

#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <prom_log.h>

#include "mach.h"

#include "ipmi_if.h"
#include "ipmi_sdr.h"
#include "ipmi_sdr_convert.h"

// KISS. Should be sufficient for expected number of 16 byte => 4 chars strings
static char *
unicode2utf8(const uint8_t *raw, uint8_t len) {
	char *res;
	uint8_t sidx = 0, didx = 0;
	const uint32_t *uc = (const uint32_t *) raw;

	char *buf = (char *) malloc((len + 1) * sizeof(char));
	if (buf == NULL)
		return NULL;
	len = len >> 2;

	while (sidx < len) {
		if (uc[sidx] <= 0x7F) {
			// Plain ASCII
			buf[didx++] = (char) uc[sidx++];
		} else if (uc[sidx] <= 0x07FF) {
			// 2-byte unicode
			buf[didx++] = (char) (0xC0 | ((uc[sidx] >> 6) & 0x1F));
			buf[didx++] = (char) (0x80 | (uc[sidx++] & 0x3F));
		} else if (uc[sidx] <= 0xFFFF) {
			// 3-byte unicode
			buf[didx++] = (char) (0xE0 | ((uc[sidx] >> 12) & 0x0F));
			buf[didx++] = (char) (0x80 | ((uc[sidx] >>  6) & 0x3F));
			buf[didx++] = (char) (0x80 | (uc[sidx++] & 0x3F));
		} else if (uc[sidx] <= 0x10FFFF) {
			// 4-byte unicode
			buf[didx++] = (char) (0xFE | ((uc[sidx] >> 18) & 0x07));
			buf[didx++] = (char) (0x80 | ((uc[sidx] >> 12) & 0x3F));
			buf[didx++] = (char) (0x80 | ((uc[sidx] >>  6) & 0x3F));
			buf[didx++] = (char) (0x80 | (uc[sidx++] & 0x3F));
		} else { 
			// invalid - use replacement character
			buf[didx++] = (char) 0xEF;
			buf[didx++] = (char) 0xBF;
			buf[didx++] = (char) 0xBD;
			sidx++;
		}
	}
	buf[didx] = '\0';
	res = strdup(buf);
	free(buf);

	return res;
}

static char *
latin12utf8(const uint8_t *raw, uint8_t len) {
	char *res;
	uint16_t sidx = 0, didx = 0;

	char *buf = (char *) malloc((len << 1 + 1) * sizeof(char));
	if (buf == NULL)
		return NULL;
	while (sidx < len) {
		if (raw[sidx] < 0x80) {
			buf[didx++] = (char) raw[sidx++];
		} else {
			// 2-byte unicode
			buf[didx++] = (char) (0xC0 | (raw[sidx] >> 6));
			buf[didx++] = (char) (0x80 | ((raw[sidx++] & 0x3F)));
		}
	}

	buf[didx] = '\0';
	res = strdup(buf);
	free(buf);

	return res;
}

static char *
bcdplus2utf8(const uint8_t *raw, uint8_t len) {
	static char BCD[] =
		{'0','1','2','3','4','5','6','7','8','9',' ','-','.',':',',','-'};
	char *res;
	uint16_t sidx, didx = 0;

	char *buf = (char *) malloc((len << 1 + 1) * sizeof(char));
	if (buf == NULL)
		return NULL;
	for (sidx = 0; sidx < len; sidx++) {
		buf[didx++] = BCD[raw[sidx] >> 4];
		buf[didx++] = BCD[raw[sidx] & 0x0F];
	}
	buf[didx] = '\0';
	res = strdup(buf);
	free(buf);

	return res;
}

static char *
ascii6p2utf8(const uint8_t *raw, uint8_t len) {
	char *res;
	uint16_t sidx, didx;
	char *buf;
	uint8_t *s;
	const uint8_t *src;

	sidx = len % 3;
	if (sidx == 0) {
		src = raw;
	} else {
		didx = len + (3 - sidx);
		s = malloc(didx * sizeof(uint8_t));
		if (s == NULL)
			return NULL;
		s[didx-1] = 0;
		s[didx-2] = 0;
		memcpy(s, raw, len);
		len = didx;
		src = s;
	}
   	buf = (char *) malloc((((len/3) << 2) + 1) * sizeof(char));
	if (buf == NULL) {
		if (src != raw)
			free(s);
		return NULL;
	}

	for (sidx = 0, didx = 0; sidx < len; sidx += 3) {
		buf[didx++] = 0x20 + (raw[sidx] & 0x3F);
		buf[didx++] = 0x20 + ((raw[sidx] >> 6) | (raw[sidx+1] & 0x0F));
		buf[didx++] = 0x20 + ((raw[sidx+1] >> 4) | (raw[sidx+2] & 0x03));
		buf[didx++] = 0x20 + (raw[sidx+3] >> 2);
	}
	buf[didx] = '\0';
	res = strdup(buf);
	free(buf);
	if (src != raw)
		free(s);

	return res;
}

char *
sdr_str2utf8(const uint8_t *raw, uint8_t len, uint8_t fmt) {
	if (raw == NULL || fmt > 3)
		return NULL;
	if (len == 0 || (len == 1 && raw[0] == 0))
		return strdup("");
	if (fmt == 0)
		return unicode2utf8(raw, len);
	else if (fmt == 3)
		return latin12utf8(raw, len);
	else if (fmt == 1)
		return bcdplus2utf8(raw, len);
	return ascii6p2utf8(raw, len);
}


/* IPMI v2, table 5-2, Generic Completion Codes (00h, C0h-FFh) */
static const char *IPMI_CC_CODES[] = {
	"Node busy",											// 0xC0
	"Invalid command",										// 0xC1
	"Invalid command on LUN",								// 0xC2
	"Timeout",												// 0xC3
	"Out of space",											// 0xC4
	"Reservation cancelled or invalid",						// 0xC5
	"Request data truncated",
	"Request data length invalid",
	"Request data field length limit exceeded",
	"Parameter out of range",
	"Cannot return number of requested data bytes",			// 0xCA
	"Requested sensor, data, or record not found",			// 0xCB
	"Invalid data field in request",
	"Command illegal for specified sensor or record type",	// 0xCD
	"Command response could not be provided",
	"Cannot execute duplicated request",
	"SDR Repository in update mode",						// 0xD0
	"Device firmeware in update mode",						// 0xD1
	"BMC initialization in progress",						// 0xD2
	"Destination unavailable",								// 0xD3
	"Insufficient privilege level",
	"Command not supported in present state",				// 0xD5
	"Cannot execute command, sub-function disabled or n/a",	// 0xD6
};

const char*
ipmi_cc2str(uint8_t code) {
	if (code == 0)
		return "Command completed normally";
	if (code < 0x80)
		return "OEM error";
	if (code < 0xC0)
		return "Command-specific error";
	code -= 0xC0;
	return (code < ARRAY_SIZE(IPMI_CC_CODES))
		? IPMI_CC_CODES[code]
		: "Unspecified error";
}

/* IPMI v2, Table 42-3, Sensor Type Codes. (42.2) */
static const char *sensor_category[] = {
	"reserved",		// 0x0
	"Temperature",
	"Voltage",
	"Current",
	"Fan",
	"Physical Security",
	"Platform Security",
	"Processor",
	"Power Supply",
	"Power Unit",
	"Cooling Device",
	"Other",
	"Memory",
	"Drive Slot / Bay",
	"POST Memory Resize",
	"System Firmwares",
	"Event Logging Disabled",
	"Watchdog1",
	"System Event",
	"Critical Interrupt",
	"Button / Switch",
	"Module / Board",
	"Microcontroller / Coprocessor",
	"Add-in Card",
	"Chassis",
	"Chip Set",
	"Other FRU",
	"Cable / Interconnect",
	"Terminator",
	"System Boot/Restart Initiated",
	"Boot Error",
	"Base OS Boot/Installation Status",
	"OS Critical Stop/Shutdown",
	"Slot / Connector",
	"System ACPI Power State",
	"Watchdog2",
	"Platform Alert",
	"Entity Presence",
	"Monitor ASIC/IC",
	"LAN",
	"Management Subsys Health",
	"Battery",
	"Session Audit",
	"Version Change",
	"FRU State"
	// 0x2D .. 0xBF reserved
	// 0xC0 .. 0xFF OEM reserved
};

const char *
sdr_category2str(uint8_t type) {
	if (type >= 0xC0)
		return "Unknown OEM";
	if (type < ARRAY_SIZE(sensor_category))
		return sensor_category[type];

	return NULL;
}

/* IPMI v2, Table 43-15, Sensor Unit Type Codes. (43.17) */
static const char *sdr_unit[] = {
	"unspecified",
	"degrees C",
	"degrees F",
	"degrees K",
	"Volts",
	"Amps",
	"Watts",
	"Joules",
	"Coulombs",
	"VA",
	"Nits",
	"lumen",
	"lux",
	"Candela",
	"kPa",
	"PSI",
	"Newton",
	"CFM",
	"RPM",
	"Hz",
	"microsecond",
	"millisecond",
	"second",
	"minute",
	"hour",
	"day",
	"week",
	"mil",
	"inches",
	"feet",
	"cu in",
	"cu feet",
	"mm",
	"cm",

	"m",
	"cu cm",
	"cu m",
	"liters",
	"fluid ounce",
	"radians",
	"steradians",
	"revolutions",
	"cycles",
	"gravities",
	"ounce",
	"pound",
	"ft-lb",
	"oz-in",
	"gauss",
	"gilberts",
	"henry",
	"millihenry",
	"farad",
	"microfarad",
	"ohms",
	"siemens",
	"mole",
	"becquerel",
	"ppm",
	"reserved",
	"decibels",
	"dbA",
	"dbC",
	"gray",
	"sievert",
	"color temp deg K",
	"bit",
	"kilobit",

	"megabit",
	"gigabit",
	"byte",
	"kilobyte",
	"megabyte",
	"gigabyte",
	"word",
	"dword",
	"qword",
	"line",
	"hit",
	"miss",
	"retry",
	"reset",
	"overflow",
	"underrun",
	"collision",
	"packets",
	"messages",
	"characters",
	"error",
	"correctable error",
#define SDR_UNIT_MAX_STRLEN 19
	"uncorrectable error",
	"fatal error",
	"grams"
};

factors_t *
sdr_factors2factors(sdr_factors_t *f) {

	if (f == NULL)
		return NULL;

	factors_t *rf = malloc(sizeof(factors_t));
	if (rf == NULL) {
		PROM_ERROR("Malloc of factors_t failed.", "");
		return NULL;
	}

	rf->linearization = f->linearization;
	rf->M = (f->M_ms & 2)
		? -512 + ((f->M_ms & 1) << 8) + f->M_ls
		: ((f->M_ms & 1) << 8) + f->M_ls;
	rf->B = (f->B_ms & 2)
		? -512 + ((f->B_ms & 1) << 8) + f->B_ls
		: ((f->B_ms & 1) << 8) + f->B_ls;
	rf->Bexp = (f->B & 8) ? -8 + (f->B & 7) : f->B; 
	rf->Rexp = (f->R & 8) ? -8 + (f->R & 7) : f->R; 
	rf->A = f->accuracy_ls | (((uint32_t) f->B_ms) << 6);
	rf->Aexp = f->accuracy_exp;

	if (ipmi_verbose > 1) {
		PROM_DEBUG("factors:\n"
			"M_ls:        %02x\n"
		 	"M_ms:        %02x   tolerance:    %02x\n"
			"B_ls:        %02x\n"
		 	"B_ms:        %02x   accuracy_ls:  %02x\n"
			"accuracy_ms: %02x   accuracy_exp: %02x   direction: %02x\n"
			"R:           %02x   B:            %02x\n"
			"M: %d   B: %d   A: %d   Rexp: %d   Bexp: %d   Aexp: %d",
			f->M_ls, f->M_ms, f->tolerance, f->B_ls, f->B_ms,
			f->accuracy_ls, f->accuracy_ms, f->accuracy_exp, f->direction,
			f->R, f->B, rf->M, rf->B, rf->A, rf->Rexp, rf->Bexp, rf->Aexp);
	}

	return rf;
}

double
sdr_convert_value(uint8_t val, uint8_t afmt, factors_t *f) {
	double res;

	// y = L[(Mx + (B * 10^K1 ) ) * 10^K2 ] units

	if (f == NULL)
		return val;

	if (afmt == 0) {
		// unsigned
		res = (double)
			(((f->M * val) + (f->B * pow(10, f->Bexp))) * pow(10, f->Rexp));
	} else if (afmt < 3) {
		if (afmt == 1 && (val & 0x80))
			val++;	// 1's complement (signed)
		// 2's complement (signed)
		res = (double)
			(((f->M * (int8_t)val)+(f->B * pow(10,f->Bexp))) * pow(10,f->Rexp));
	} else {
		PROM_WARN("Not a analog (numeric) reading (%d).", afmt);
		return val;
	}

	switch (f->linearization) {
		case SDR_LTYPE_LN:
			return log(res);
		case SDR_LTYPE_LOG10:
			return log10(res);
		case SDR_LTYPE_LOG2:
			return (double) (log(res) / log(2.0));
		case SDR_LTYPE_E:
			return exp(res);
		case SDR_LTYPE_EXP10:
			return pow(10.0, res);
		case SDR_LTYPE_EXP2:
			return pow(2.0, res);
		case SDR_LTYPE_1_X:
			return pow(res, -1.0);	/*1/x w/o exception */
		case SDR_LTYPE_SQR:
			return pow(res, 2.0);
		case SDR_LTYPE_CUBE:
			return pow(res, 3.0);
		case SDR_LTYPE_SQRT:
			return sqrt(res);
		case SDR_LTYPE_CUBERT:
			return cbrt(res);
		case SDR_LTYPE_LINEAR:
		default:
			break;	// e.g. non-linear values get not linearized
	}

	return res;
}

const char *
sdr_unit2str(unit_t *u) {
	// base + modifier + mprefix + '\0'
	static char buf[2 * SDR_UNIT_MAX_STRLEN + 2 + 1];
	char *idx = buf;
	int len;

	const char *sbase = u->base == 0
		? ""
		: (u->base < ARRAY_SIZE(sdr_unit)) ? sdr_unit[u->base] : "???";
	const char *smod = u->modifier == 0
		? ""
		: (u->modifier < ARRAY_SIZE(sdr_unit)) ? sdr_unit[u->modifier] : "???";

	if (u->is_percent)
		return "percent";		// this is closer to prom names than '%'

	len = strlen(sbase);
	strncpy(idx, sbase, len);
	idx += len;

	if (u->modifier_prefix == SDR_UNIT_MODIFIER_PREFIX_MUL) {
		*idx = '*';
		idx++;
	} else if (u->modifier_prefix == SDR_UNIT_MODIFIER_PREFIX_DIV) {
		*idx = '/';
		idx++;
	}

	len = strlen(smod);
	strncpy(idx, smod, len);
	idx += len;
	*idx = '\0';

	return buf;
}
