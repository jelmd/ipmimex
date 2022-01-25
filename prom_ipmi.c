/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License") 1.1!
 * You may not use this file except in compliance with the License.
 *
 * See  https://spdx.org/licenses/CDDL-1.1.html  for the specific
 * language governing permissions and limitations under the License.
 *
 * Copyright 2021 Jens Elkner (jel+ipmimex-src@cs.ovgu.de)
 */
#include <stdio.h>
#include <string.h>

#include <prom_string_builder.h>

#include "common.h"
#include "ipmi_sdr.h"
#include "ipmi_sdr_convert.h"
#include "prom_ipmi.h"

void
collect_ipmi(psb_t *sb, sensor_t *slist) {
	sdr_reading_t *r;
	sdr_factors_t *f;
	factors_t *rf;
	uint8_t value, cc, tstate;
	double real_val;
	char buf[512];
	size_t sz;
	bool free_sb = sb == NULL;
	sensor_t *s = slist;

	if (slist == NULL)
		return;

	if (free_sb) {
		sb = psb_new();
		if (sb == NULL) {
			perror("collect_ipmi: ");
			return;
		}
	}
	sz = psb_len(sb);

	while (s != NULL) {
		if (s->prom.note != NULL)
			psb_add_str(sb, s->prom.note);
		r = get_reading(s->sensor_num, s->name, &cc);
		if (r == NULL || cc != 0 || r->unavailable || !r->scanning_enabled)
			goto next;
		value = r->value;
		tstate = r->state0 & 0x3F;
		if (SDR_LTYPE_IS_NON_LINEAR(s->factors->linearization)) {
			f = get_factors(s->sensor_num, value, &cc);
			if (f == NULL)
				goto next;
			rf = sdr_factors2factors(f);
			if (rf == NULL)
				goto next;
		} else {
			rf = s->factors;
		}
		real_val = sdr_convert_value(value, s->unit.analog_fmt, rf);
		psb_add_str(sb, s->prom.mname_reading);
		sprintf(buf, s->prom.unit[0] == 'V' ? " %g\n" : " %g\n", real_val);
		psb_add_str(sb, buf);
		if (s->prom.mname_state != NULL) {
			psb_add_str(sb, s->prom.mname_state);
			sprintf(buf, " %d\n", tstate == 0
				? 0
				: ((tstate >= 8) ? (tstate >> 3) : - tstate));
			psb_add_str(sb, buf);
		}
		if (s->prom.mname_threshold != NULL)
			psb_add_str(sb, s->prom.mname_threshold);
next:
		s = s->next;
	}

	if (free_sb) {
		if (psb_len(sb) != sz)
			fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
}

void
collect_dcmi(psb_t *sb, bool compact, bool no_powerstats) {
	uint8_t cc;
	char buf[256];
	size_t sz = 0;
	bool free_sb = sb == NULL;

	if (free_sb) {
		sb = psb_new();
		if (sb == NULL) {
			perror("collect_dcmi: ");
			return;
		}
		sz = psb_len(sb);
	}

	if (!compact)
		addPromInfo(IPMIMEXM_DCMI_POWER);

	sdr_power_t *p = get_power(&cc);
	if (p == NULL || cc != 0)
		return;
	psb_add_str(sb, IPMIMEXM_DCMI_POWER_N "{value=\"now\"} ");
	sprintf(buf, "%d\n", p->curr);
	psb_add_str(sb, buf);

	if (!no_powerstats) {
		psb_add_str(sb, IPMIMEXM_DCMI_POWER_N "{value=\"min\"} ");
		sprintf(buf, "%d\n", p->min);
		psb_add_str(sb, buf);
		psb_add_str(sb, IPMIMEXM_DCMI_POWER_N "{value=\"max\"} ");
		sprintf(buf, "%d\n", p->max);
		psb_add_str(sb, buf);
		psb_add_str(sb, IPMIMEXM_DCMI_POWER_N "{value=\"avg\"} ");
		sprintf(buf, "%d\n", p->avg);
		psb_add_str(sb, buf);

		if (!compact)
			addPromInfo(IPMIMEXM_DCMI_PSAMPLE);
		psb_add_str(sb, IPMIMEXM_DCMI_PSAMPLE_N);
		sprintf(buf, " %u\n", p->sample_time/1000);
		psb_add_str(sb, buf);
	}

	if (free_sb) {
		if (psb_len(sb) != sz)
			fprintf(stdout, "\n%s", psb_str(sb));
		psb_destroy(sb);
	}
}

/**
 * @brief	Metric names. Keep in sync with IPMI v2, Table 42-3, Sensor Type
 *	Codes (42.2).
 */
static const char *metric_name[] = {
	"reserved",		// 0x0
	"temperature",
	"voltage",
	"current",
	"fan_speed",
	"physical_security",
	"platform_security",
	"processor",
	"power_supply",
	"power_unit",
	"cooling_device",
	"sensor",
	"memory",
	"bay",
	"post_memory_resize",
	"system_fw",
	"sel_disabled",
	"watchdog1",
	"sys_event",
	"critical_interrupt",
	"button",
	"module",
	"coproc",
	"add_in_card",
	"chassis",
	"chip",
	"fru",
	"cable",
	"terminator",
	"sys_boot",
	"boot_error",
	"os_boot",
	"os_critical_stop",
	"slot",
	"system_acpi_power",
	"watchdog2",
	"platform_alert",
	"presence",
	"monitor_ic",
	"lan",
	"management_subsys_health",
	"battery",
	"session_audit",
	"version_change",
	"fru_state"
	// 0x2D .. 0xBF reserved
	// 0xC0 .. 0xFF OEM reserved
};

/**
 * @brief	Metric units. Keep in sync with IPMI v2, Table 43-15, Sensor Unit
 *	Type Codes. (43.17)
 */
static const char *metric_unit[] = {
	"",
	"C",
	"F",
	"K",
	"V",
	"A",
	"W",
	"J",
	"C",
	"VA",
	"nt",
	"lm",
	"lx",
	"cd",
	"kPa",
	"psi",
	"N",
	"cfm",
	"rpm",
	"hz",
	"us",
	"ms",
	"s",
	"min",
	"h",
	"d",
	"w",
	"mil",
	"in",
	"ft",
	"cin",
	"cft",
	"mm",
	"cm",

	"m",
	"ccm",
	"cm",
	"l",
	"floz",
	"rad",
	"sr",
	"revolutions",
	"cycles",
	"gravities",
	"oz",
	"pound",
	"ftlb",
	"ozin",
	"G",
	"Gb",
	"H",
	"mH",
	"F",
	"mF",
	"ohms",
	"S",
	"mol",
	"Bq",
	"ppm",
	"reserved",
	"db",
	"dbA",
	"dbC",
	"Gy",
	"Sv",
	"color_K",
	"bits",
	"kbits",

	"Mbits",
	"Gbits",
	"bytes",
	"kB",
	"MB",
	"GB",
	"words",
	"dwords",
	"qwords",
	"lines",
	"hits",
	"misses",
	"retry",
	"resets",
	"overflows",
	"underruns",
	"collisions",
	"pkts",
	"msgs",
	"chars",
	"errors",
	"correctable errors",
#define PROM_UNIT_MAX_STRLEN 20
	"uncorrectable_errors",
	"fatal_errors",
	"g"
};

const char *
unit2prom(unit_t *u) {
	// base + modifier + mprefix + '\0'
	static char buf[2 * PROM_UNIT_MAX_STRLEN + 5 + 1];
	char *idx = buf;
	size_t len;

	const char *sbase = u->base == 0
		? ""
		: (u->base < ARRAY_SIZE(metric_unit)) ? metric_unit[u->base] : "";
	const char *smod = u->modifier == 0
		? ""
		: (u->modifier < ARRAY_SIZE(metric_unit)) ? metric_unit[u->modifier]:"";

	if (u->is_percent)
		return "percent";       // this is closer to prom names than '%'

	len = strlen(sbase);
	strcpy(idx, sbase);
	idx += len;

	if (u->modifier_prefix == SDR_UNIT_MODIFIER_PREFIX_MUL) {
		*idx = 'x';
		idx++;
	} else if (u->modifier_prefix == SDR_UNIT_MODIFIER_PREFIX_DIV) {
		strcpy(idx, "_per_");
		idx += 5;
	}

	len = strlen(smod);
	strcpy(idx, smod);
	idx += len;
	*idx = '\0';

	return buf;
}

const char *
category2prom(uint8_t code) {
	if (code >= 0xC0)
		return "unknown_oem";
	if (code < ARRAY_SIZE(metric_name))
		return metric_name[code];

	return NULL;
}
