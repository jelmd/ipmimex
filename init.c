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

#include <prom.h>

#include "init.h"

static uint started = 0;

uint
start(void) {
	if (started)
		return 0;

	PROM_DEBUG("IPMI stack initialized", "");
	started = 1;
	return 0;
	/*
	PROM_WARN("Failed to initialize IPMI");
	return 1;
	*/
}

uint
stop(void) {
	PROM_DEBUG("IPMI stack has been properly shut down", "");
	started = 0;
	return 0;
	/*
	PROM_WARN("Failed to shutdown IPMI");
	return 1;
	*/
}

static char *versionProm = NULL;
static char *versionHR = NULL;

char *
getVersions(psb_t *sbp, bool compact) {
	psb_t *sbi = NULL, *sb = NULL;

	if (versionProm != NULL)
		goto end;

	sbi = psb_new();
	sb = psb_new();
	if (sbi == NULL || sb == NULL) {
		psb_destroy(sbi);
		psb_destroy(sb);
		return NULL;
	}

	if (!compact)
		addPromInfo(IPMIMEXM_VERS);

	versionHR = psb_dump(sbi);
	psb_destroy(sbi);
	versionProm = psb_dump(sb);
	psb_destroy(sb);

end:
	if (sbp == NULL) {
		fprintf(stdout, "\n%s", versionProm);
	} else {
		psb_add_str(sbp, versionProm);
	}
	return versionHR;
}
