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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include <prom_string_builder.h>
#include <prom_log.h>

#include "ipmi_if.h"
#include "ipmi_sdr.h"

#define WAIT4REPO_SLOT	10			// seconds
#define MAX_WAIT4REPO	300			// seconds


static struct option sdr_opts[] = {
	{ "ignore",		no_argument,	NULL, 'D'},
	{ "drop-noread",no_argument,	NULL, 'N'},
	{ "help",		no_argument,	NULL, 'h'},
	{ "loglevel",	no_argument,	NULL, 'l'},
	{ "verbose",	no_argument,	NULL, 'v'},
	{ "extended",	no_argument,	NULL, 'x'},
	{0, 0, 0, 0}
};
static const char *sdr_short_opts  = { "DNhl:vx" };
static const char *sdr_short_usage = { "[-DNhvx]i [-l {DEBUG|INFO|WARN|ERROR}"};

int
main(int argc, char **argv) {
	uint32_t sensors;
	uint8_t res = 0, cc;
	struct timespec start, end;
	int max_tries;
	sensor_t *slist = NULL;
	bool ignore_disabled_flag = false, extended = false, drop_noread = false;

	while (1) {
		int c, optidx = 0;
		c = getopt_long (argc, argv, sdr_short_opts, sdr_opts, &optidx);
		if (c == -1)
			break;
		switch (c) {
			case 'D':
				ignore_disabled_flag = true;
				break;
			case 'N':
				drop_noread = true;
				break;
			case 'v':
				prom_log_level(PLL_DBG);
				ipmi_verbose++;
				break;
			case 'l':
				cc = prom_log_level_parse(optarg);
				if (cc == 0)
					fprintf(stderr,"Invalid log level '%s' ignored.\n",optarg);
				else {
					prom_log_level(cc);
					if (cc ==  PLL_DBG)
						ipmi_verbose++;
				}
				break;
			case 'x':
				extended = true;
				break;
			case 'h':
			case '?':
				fprintf(stderr, "Usage: %s %s\n", argv[0], sdr_short_usage);
				return (1);
		}
	}

	if ((res = ipmi_if_open(NULL)) != 0)
		return 99;

	max_tries = MAX_WAIT4REPO/WAIT4REPO_SLOT;
	while (max_tries > 0) {
		ipmi_bmc_info_t *bmc = get_bmc_info(&cc);
		if (SDR_REPO_TMP_NA(cc)) {
			sleep(WAIT4REPO_SLOT);
			max_tries--;
			continue;
		}
		if (bmc == NULL) {
			PROM_WARN("  Could not obtain BMC info!!!\n"
"  If it is not IPMI v1.0, v1.5 or v2.0 compatible, shown results (if any)\n"
"  might be total non-sense.\n", "");
		} else if (!bmc->supports_sensor) {
			PROM_ERROR("BMC does not support SDR sensor device commands.", "")
			res = 98;
			goto end;
		}
		break;
	}

	int r;
	time_t s;
	long ns;
	double duration;
	max_tries = MAX_WAIT4REPO/WAIT4REPO_SLOT;
	while (max_tries > 0) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		slist = scan_sdr_repo(&sensors, ignore_disabled_flag, drop_noread, &cc);
		if (SDR_REPO_TMP_NA(cc)) {
			free_sensor(slist);
			slist = NULL;
			sleep(WAIT4REPO_SLOT);
			max_tries--;
			continue;
		}
		if (cc == 0) {
			r = clock_gettime(CLOCK_MONOTONIC, &end);
			s = (r == 0) ? end.tv_sec - start.tv_sec : 0;
			ns = (r == 0) ? end.tv_nsec - start.tv_nsec : 0;
			duration = s + ns*1e-9;
			PROM_INFO("Sensor list population took %f seconds.", duration);
			PROM_INFO("Using %d sensors", sensors);
		} else {
			res = 97;
			goto end;
		}
		break;
	}
	clock_gettime(CLOCK_MONOTONIC, &start);
	show_ipmitool_sensors(slist, NULL, extended);
	r = clock_gettime(CLOCK_MONOTONIC, &end);
	s = (r == 0) ? end.tv_sec - start.tv_sec : 0;
	ns = (r == 0) ? end.tv_nsec - start.tv_nsec : 0;
	duration = s + ns*1e-9;
	PROM_INFO("Getting/printing sensor values took %f seconds.", duration);

	clock_gettime(CLOCK_MONOTONIC, &start);
	if (!sdrs_changed(slist)) {
		r = clock_gettime(CLOCK_MONOTONIC, &end);
		s = (r == 0) ? end.tv_sec - start.tv_sec : 0;
		ns = (r == 0) ? end.tv_nsec - start.tv_nsec : 0;
		duration = s + ns*1e-9;
		PROM_INFO("SDR change check took %f seconds.", duration);
	} else {
		PROM_DEBUG("1+ SDR changed.", "");
	}
	// 2nd time should be shorter because no list scanning
	clock_gettime(CLOCK_MONOTONIC, &start);
	if (!sdrs_changed(slist)) {
		r = clock_gettime(CLOCK_MONOTONIC, &end);
		s = (r == 0) ? end.tv_sec - start.tv_sec : 0;
		ns = (r == 0) ? end.tv_nsec - start.tv_nsec : 0;
		duration = s + ns*1e-9;
		PROM_INFO("SDR change check2 took %f seconds.", duration);
	} else {
		PROM_DEBUG("1+ SDR changed.", "");
	}
end:
	ipmi_if_close();
	free_sensor(slist);
	return res;
}
