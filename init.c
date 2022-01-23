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
#include <unistd.h>
#include <stdlib.h>

#include <prom.h>

#include "common.h"
#include "init.h"
#include "ipmi_if.h"
#include "ipmi_sdr.h"
#include "ipmi_sdr_convert.h"

#include "prom_ipmi.h"

static uint8_t started = 0;

#define WAIT4REPO_SLOT	10			// seconds
#define MAX_WAIT4REPO	300			// seconds

static char *versionProm = NULL;	// version string emitted via /metrics
static char *versionHR = NULL;		// version string emitted to stdout/stderr
static char *bmcVersion = NULL;
static bool bmc_version_done = false;

static int
cmp_sensor(const void *p1, const void *p2) {
	const sensor_t *a = *(sensor_t * const *)p1;
	const sensor_t *b = *(sensor_t * const *)p2;
	int d = a->category - b->category;
	if (d !=0)
		return d;
	d = strcmp(a->prom.unit, b->prom.unit);
	if (d != 0)
		return d;
	return strcmp(a->prom.name, b->prom.name);
}

static sensor_t *
sort_sensors(sensor_t *list, size_t sz) {
	size_t n;
	int len;
	char buf[64];	// metric name buffer - should be more than sufficient

	if (list == NULL)
		return NULL;

	sensor_t *e, **sa = malloc(sizeof(sensor_t *) * sz);
	if (sa == NULL) {
		perror("sort sensors: ");
		return NULL;
	}
	e = list;
	n = 0;
	while (e != NULL) {
		sa[n] = e;

		strcpy(buf, e->name);
		len = strlen(buf);
		if ((e->category == 1) && ((strcmp(buf + len - 5, " Temp") == 0)
			|| (strcmp(buf + len - 5, "_TEMP") == 0)))
		{
			buf[len-5] = '\0';
		}
		// just enough to sort prom output like
		e->prom.name = strdup(buf);
		e->prom.unit = strdup(unit2prom(&(e->unit)));

		e = e->next;
		n++;
	}
	if (sz != n) {
		PROM_FATAL("Software bug: sz != c (%ld != %ld)", sz, n);
		free(sa);
		return NULL;
	}
	qsort(sa, sz, sizeof(sensor_t *), cmp_sensor);
	for (n = sz - 1; n > 0 ; n--) {
		sa[n - 1]->next = sa[n];
	}
	sa[sz - 1]->next = NULL;
	e = sa[0];
	free(sa);
	return e;
}

#define MMATCH(_x)	(cfg->_x && (regexec(cfg->_x, buf, 0,NULL,0) == 0))
#define SMATCH(_x)	(cfg->_x && (regexec(cfg->_x, e->prom.name, 0,NULL,0) == 0))

static sensor_t *
drop_unneeded(sensor_t *head, scan_cfg_t *cfg, uint32_t *sensors) {
	if (head == NULL)
		return NULL;

	sensor_t *e = head, *first = NULL, *last = NULL, *tmp;
	char buf[128];		// 8+1+32+1+20+5+20+20+16+1 = 124
	char tbuf[4096];	// 6*(124 + 27 + 317) = 2808
	int len, ulen;
	uint8_t cc;

	while (e != NULL) {
		len = sprintf(buf, IPMIMEXM_IPMI_N "_%s_%s",
			category2prom(e->category), e->prom.unit);
		if ((MMATCH(exc_metrics) || SMATCH(exc_sensors))
			&& !(MMATCH(inc_metrics) || SMATCH(inc_sensors)))
		{
			PROM_INFO("Dropping sensor '%s' (0x%02x): excluded via -x or -X.",
				e->prom.name, e->sensor_num);
			tmp = e->next;
			if (last != NULL)
				last->next = tmp;
			e->next = NULL;
			free_sensor(e);
			e = tmp;
			(*sensors)--;
			continue;
		}
		if (first == NULL)
			first = e;
		last = e;
		ulen = len - strlen(e->prom.unit);
		sprintf(buf + len, "{sensor=\"%s\"}", e->prom.name);
		e->prom.mname_reading = strdup(buf);

		if (!cfg->no_state) {
			sprintf(buf + ulen, "state{sensor=\"%s\"}", e->prom.name);
			e->prom.mname_state = strdup(buf);
		}

		sdr_thresholds_t *t = cfg->no_thresholds
			? NULL
			: get_thresholds(e->sensor_num, &cc);
		if (t != NULL && cc == 0) {
			sprintf(buf + ulen, "threshold_%s{sensor=\"%s\",bounds=",
				e->prom.unit, e->prom.name);
			len = 0;

#define TADD(_b, _s)	if (t->readable._b ## _ ## _s) { \
	len += (SDR_UNIT_FMT_IS_DISCRETE(e->unit.analog_fmt)) \
	? sprintf(tbuf + len, "%s\"" #_b "\",state=\"" #_s "\"} %d\n", buf, \
		t->_b ## _ ## _s) \
	: sprintf(tbuf + len, "%s\"" #_b "\",state=\"" #_s "\"} %g\n", buf, \
		sdr_convert_value(t->_b ## _ ## _s, e->unit.analog_fmt, e->factors)); \
}
			TADD(lower, nr);
			TADD(lower, cr);
			TADD(lower, nc);
			TADD(upper, nc);
			TADD(upper, cr);
			TADD(upper, nr);
			if (len > 0) {
				e->prom.mname_threshold = strdup(tbuf);
				e->it_thresholds =
					thresholds2ipmitool_str(t, e->unit.analog_fmt, e->factors);
			}
		}
		e = e->next;
	}
	return first;
}

static uint8_t
get_current_bmc_info(void) {
	int max_tries;
	uint8_t cc;
	char buf[256];

	max_tries = MAX_WAIT4REPO/WAIT4REPO_SLOT;
	while (max_tries > 0) {
		ipmi_bmc_info_t *bmc = get_bmc_info(&cc);
		if (SDR_REPO_TMP_NA(cc)) {
			PROM_INFO("BMC temporarily not available. Sleeping %d seconds ...",
				WAIT4REPO_SLOT);
			sleep(WAIT4REPO_SLOT);
			max_tries--;
			continue;
		}
		if (bmc == NULL) {
			PROM_WARN("\n  Could not obtain BMC info!!!\n"
			"  If it is not IPMI v1.0, v1.5 or v2.0 compatible, shown results\n"
			"  (if any) might be total non-sense.\n", "");
			return 1;
		} else if (!bmc->supports_sensor) {
			PROM_ERROR("BMC does not support SDR sensor device commands.", "");
			return 2;
		}
		sprintf(buf, IPMIMEXM_VERS_N "{name=\"bmc\",value=\"%d.%d\"} 1\n",
			bmc->fw_rev_major, bmc->fw_rev_minor);
		if (bmcVersion != NULL)
			free(bmcVersion);
		bmcVersion = strdup(buf);
		break;
	}
	return max_tries == 0 ? 3 : 0;
}

sensor_t *
get_sensor_list(scan_cfg_t *cfg, uint32_t *sensors) {
	int max_tries;
	uint8_t cc;

	if (cfg->no_ipmi)
		return NULL;

	sensor_t *slist = NULL, *tlist;
	max_tries = MAX_WAIT4REPO/WAIT4REPO_SLOT;
	while (max_tries > 0) {
		*sensors = 0;
		slist = scan_sdr_repo(sensors, cfg->ignore_disabled_flag,
			cfg->drop_no_read, &cc);
		if (SDR_REPO_TMP_NA(cc)) {
			free_sensor(slist);
			slist = NULL;
			PROM_INFO("BMC temporarily not available. Sleeping %d seconds ...",
				WAIT4REPO_SLOT);
			sleep(WAIT4REPO_SLOT);
			max_tries--;
			continue;
		}
		if (cc == 0) {
			PROM_INFO("%d potential sensors found.", *sensors);
			break;
		} else {
			PROM_FATAL("Scanning the SDR repository failed. No sensors.", "");
			goto error;
		}
	}
	if (max_tries == 0) {
		*sensors = 0;
		return NULL;
	}

	tlist = sort_sensors(slist, *sensors);
	if (tlist != NULL) {
		slist = tlist;
		tlist = drop_unneeded(slist, cfg, sensors);
		if (tlist != NULL)
			return tlist;
		PROM_WARN("No sensors to monitor.", "");
	}

error:
	free_sensor(slist);
	slist = NULL;
	*sensors = 0;
	return NULL;
}

/**
 * This function expects a sensor list sorted by category, prom.unit and
 * prom.name and a prom.mname_reading to be populated for all list members.
 * If not run through drop_unneeded() or any field is NULL expect coredumps.
 */
static void
gen_help(sensor_t *list) {
	if (list == NULL)
		return;

	sensor_t *s = list, *last = NULL;
	char buf[256];
	size_t len;
	const char *u;
	char *t;

	while (s != NULL) {
		if (last == NULL
			|| strncmp(last->prom.mname_reading,s->prom.mname_reading,len) != 0)
		{
			u = (s->it_unit) ? s->it_unit : sdr_unit2str(&(s->unit));
			t = strchr(s->prom.mname_reading, '{');
			*t = '\0';
			sprintf(buf, "\n# HELP %s IPMI %s sensor in %s\n# TYPE %s %s\n",
				s->prom.mname_reading, sdr_category2str(s->category), u,
				s->prom.mname_reading, IPMIMEXM_IPMI_T);
			*t = '{';
			s->prom.note = strdup(buf);
			len = t - s->prom.mname_reading + 1;
			last = s;
		}
		s = s->next;
	}
}

sensor_t *
start(scan_cfg_t *cfg, bool compact, uint32_t *sensors) {
	uint8_t cc;

	*sensors = 0;
	if (started)
		return NULL;

	if (cfg->no_ipmi && cfg->no_dcmi)
		return NULL;

	PROM_INFO("Checking BMC (%s) ...",
		cfg->bmc == NULL ? "default path" : cfg->bmc);
	if (ipmi_if_open(cfg->bmc) != 0)
		return NULL;

	cc = get_current_bmc_info();
	if (cc == 2) {
		cfg->no_ipmi = true;
	} else if (cc == 3) {
		ipmi_if_close();
		return NULL;
	}

	sensor_t *slist = get_sensor_list(cfg, sensors);
	if (*sensors == 0)
		cfg->no_ipmi = true;
	else if (!compact)
		gen_help(slist);

	if (!cfg->no_dcmi) {
		get_power(&cc);
		if (cc == SDR_CC_INVALID_CMD)
			cfg->no_dcmi = true;
	}
	if (cfg->no_ipmi && cfg->no_dcmi) {
		ipmi_if_close();
		return NULL;
	}

	//show_ipmitool_sensors(slist, NULL, true);
	if (!cfg->no_dcmi)
		(*sensors)++;

	PROM_INFO("IPMI stack initialized. All sensors to monitor: %d", *sensors);
	started = 1;
	return slist;
}

void
stop(sensor_t *list) {
	ipmi_if_close();
	free_sensor(list);
	list = NULL;
	free(versionHR);
	versionHR = NULL;
	free(versionProm);
	versionProm = NULL;
	free(bmcVersion);
	bmcVersion = NULL;
	bmc_version_done = false;
	PROM_DEBUG("IPMI stack has been properly shutdown", "");
	started = 0;
}

char *
getVersions(psb_t *sbp, bool compact) {
	psb_t *sbi = NULL, *sb = NULL;

	if (versionProm != NULL) {
		if (!bmc_version_done && bmcVersion != NULL) {
			size_t n = strlen(versionProm), m = strlen(bmcVersion);
			char *t, *s = malloc(sizeof(char) * (n + m) + 1);
			if (s != NULL) {
				strcpy(s, versionProm);
				strcpy(s + n, bmcVersion);
				t = versionProm;
				versionProm = s;
				free(t);
				bmc_version_done = true;
			}
		}
		goto end;
	}

	sbi = psb_new();
	sb = psb_new();
	if (sbi == NULL || sb == NULL) {
		psb_destroy(sbi);
		psb_destroy(sb);
		return NULL;
	}

	psb_add_str(sbi, "ipmimex " IPMIMEX_VERSION "\n(C) 2021 "
		IPMIMEX_AUTHOR "\n");
	versionHR = psb_dump(sbi);
	psb_destroy(sbi);

	if (!compact)
		addPromInfo(IPMIMEXM_VERS);

	psb_add_str(sb, IPMIMEXM_VERS_N "{name=\"server\",value=\"" IPMIMEX_VERSION
		"\"} 1\n");

	versionProm = psb_dump(sb);
	psb_destroy(sb);

end:
	if (sbp == NULL) {
		fprintf(stdout, "%s", versionHR);
	} else {
		psb_add_str(sbp, versionProm);
	}
	return versionHR;
}
