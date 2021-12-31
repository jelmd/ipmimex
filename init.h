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
 * @return \c 0 on success, a number > 0 otherwise.
 */
uint start(void);

/**
 * @brief Shutdown the IPMI stack.
 * @return \c 0 on success, a number > 0 otherwise.
 */
uint stop(void);

char *getVersions(psb_t *report, bool compact);

#ifdef __cplusplus
}
#endif

#endif	// IPMIMEX_INIT_H
