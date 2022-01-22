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

/**
 * @file init.h
 * Utilities to initialize IPMI related stuff.
 */

#ifndef IPMIMEX_INIT_H
#define IPMIMEX_INIT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize IPMI stack.
 * @param cfg	The SDR scan configuration to use.
 * @param compact	If \c true, no HELP/TYPE comments get generated and thus
 *	will not be emitted in a client response.
 * @return \c NULL on error, the sensor list otherwise.
 */
sensor_t *start(scan_cfg_t *cfg, bool compact);

/**
 * @brief Shutdown the IPMI stack and cleanup any allocated resources (and
 *	prepare for exit).
 * @param list	The list of sensors to release.
 * @return \c 0 on success, a number > 0 otherwise.
 */
void stop(sensor_t *list);

char *getVersions(psb_t *report, bool compact);

#ifdef __cplusplus
}
#endif

#endif	// IPMIMEX_INIT_H
