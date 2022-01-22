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

#ifndef IPMIMEX_COMMON_H
#define IPMIMEX_COMMON_H

#define IPMIMEX_VERSION "1.0.0"
#define IPMIMEX_AUTHOR "Jens Elkner (jel+ipmimex@cs.uni-magdeburg.de)"

#include <stdbool.h>
#include <regex.h>

#include <prom_string_builder.h>
#include <prom_log.h>

#include "ipmi_sdr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBUF_SZ 256

typedef struct scan_cfg {
	char *bmc;
	bool drop_no_read;
	bool ignore_disabled_flag;
	bool no_state;
	bool no_thresholds;
	bool no_ipmi;
	bool no_dcmi;
	regex_t *exc_metrics;
	regex_t *exc_sensors;
	regex_t *inc_metrics;
	regex_t *inc_sensors;
} scan_cfg_t;

#define addPromInfo(metric) {\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# HELP " metric ## _N " " metric ## _D );\
	psb_add_char(sb, '\n');\
	psb_add_str(sb, "# TYPE " metric ## _N " " metric ## _T);\
	psb_add_char(sb, '\n');\
}

#define IPMIMEXM_VERS_D "Software version information."
#define IPMIMEXM_VERS_T "gauge"
#define IPMIMEXM_VERS_N "ipmimex_version"

#define IPMIMEXM_IPMI_D "IPMI sensor "
#define IPMIMEXM_IPMI_T "gauge"
#define IPMIMEXM_IPMI_N "ipmimex_ipmi"

#define IPMIMEXM_DCMI_POWER_D "DCMI power reading in Watt."
#define IPMIMEXM_DCMI_POWER_T "gauge"
#define IPMIMEXM_DCMI_POWER_N "ipmimex_dcmi_power_W"

#define IPMIMEXM_DCMI_PSAMPLE_D "DCMI sample period for min, max and average power in seconds."
#define IPMIMEXM_DCMI_PSAMPLE_T "gauge"
#define IPMIMEXM_DCMI_PSAMPLE_N "ipmimex_dcmi_power_sample_seconds"
/*
#define IPMIMEXM_XXX_D "short description."
#define IPMIMEXM_XXX_T "gauge"
#define IPMIMEXM_XXX_N "ipmimex_yyy"

 */

#ifdef __cplusplus
}
#endif

#endif // IPMIMEX_COMMON_H
