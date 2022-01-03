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
 * @file ipmi_if.h
 * IPMI interface related definitions.
 * @note	The interface has **no** multi-threading in mind because the BMCs
 *			itself as well as the related OS driver are single threaded, too.
 *			So make sure to call one function after another and be aware, that
 *			a function may change the underlying buffer of a received message
 *			returned in a previous call.
 */

#ifndef IPMIMEX_IF_H
#define IPMIMEX_IF_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	IPMI message to send to the OS driver.
 */
struct ipmi_rq {
	struct {
		uint8_t netfn:6;
		uint8_t lun:2;
		uint8_t cmd;
		uint16_t data_len;
		uint8_t *data;
	} msg;
};

/**
 * @brief	IPMI message received from the OS driver incl. meta data.
 */
struct ipmi_rs {
	/**< completion code */
	uint8_t ccode;
	/**< buffer for returned ipmi message. Full sdr records are < 64 bytes.
      * But just in case their might be some indirections etc. we make it a
      * little bit bigger: 4 * 256 bytes */
	uint8_t data[1024];
	/**< actual size of the ipmi message returned in the buffer */
	int data_len;
};

/**
 * @brief	Open the given IPMI device \c dev so that it can be used with
 *		\c ipmi_send() and \c ipmi_recv(), otherwise such calls will fail.
 *		When done, one should call \c ipmi_if_close() to close the related
 *		device and free related resources.
 * @param dev	The device to open. If \c NULL the default device (Linux
 *		\c /dev/ipmi0 and Solaris \c /dev/bmc) will be used instead.
 * @return \c 0 on success, a value \c != \c 0 otherwise.
 */
int ipmi_if_open(char *dev);

/**
 * @brief	Close the already opened IPMI device. Ignored if the related device
 *		got closed previously.
 */
void ipmi_if_close(void);

/**
 * @brief	Send the given IPMI request to the already opened IPMI device.
 * @param req	The request to send.
 * @returns	On success the id of the message sent, which is always \c >= \c 0,
 *	a value \c < \c 0 otherwise.
 */
int ipmi_send(struct ipmi_rq *req);

/**
 * @brief Fetch the answer for the IPMI request with the given \c msgid.
 * @param msgid	  Fetch the answer for the IMPI request with the given \c msgid.
 *		If no IPMI request with such an ID has been sent before, this function
 *		will keep fetching answers 'til it finds one with the given \c msgid,
 *		which probably means, it will hit the timeout.
 * @param timeout	Max. number of seconds to wait for an answer. A value
 *		\c <= \c 0 gets replaced by the internal default.
 * @return \c NULL on error, timeout or no received data, a pointer to the
 *		buffer of the retrieved data otherwise. NOTE that the buffer gets
 *		overwritten by the next call of this function.
 */
struct ipmi_rs *ipmi_recv(long msgid, long timeout);

#ifdef __cplusplus
}
#endif

#endif	// IPMIMEX_IF_H
