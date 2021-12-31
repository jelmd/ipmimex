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

#define IPMIMEX_VERSION "0.0.1"
#define IPMIMEX_AUTHOR "Jens Elkner (jel+ipmimex@cs.uni-magdeburg.de)"

#ifndef uint
#define uint unsigned int
#endif

#include <stdbool.h>

#include <prom_string_builder.h>
#include <prom_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBUF_SZ 256

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

/*
#define IPMIMEXM_XXX_D "short description."
#define IPMIMEXM_XXX_T "gauge"
#define IPMIMEXM_XXX_N "ipmimex_yyy"

 */

#ifdef __cplusplus
}
#endif

#endif // IPMIMEX_COMMON_H
