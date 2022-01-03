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

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#include <time.h>
#include <sys/bmc_intf.h>			// pkg:/system/header

#include "common.h"
#include "ipmi_if.h"

#ifdef DEBUG_IPMI_IF
#include "hexdump.h"
#endif

extern int sdr_verbose;

static bool is_open = false;
static char *ipmi_dev = NULL;
static int ipmi_fd = -1;

int
ipmi_if_open(char *dev) {
	if (is_open) {
		PROM_WARN("IPMI device '%s' already open.", ipmi_dev);
		return 0;
	}
	ipmi_dev = (dev == NULL) ? strdup("/dev/bmc") : strdup(dev);
	PROM_INFO("Using IPMI device '%s' ...", ipmi_dev);
	ipmi_fd = open(ipmi_dev, O_RDWR | O_NONBLOCK);
	if (ipmi_fd < 0) {
		PROM_FATAL("Unable to open '%s' in RW mode.", ipmi_dev);
		free(ipmi_dev);
		ipmi_dev = NULL;	
		return 1;
	}
	is_open = true;

	return 0;
}

void
ipmi_if_close(void) {
	if (is_open && ipmi_fd >= 0) {
		PROM_DEBUG("Closing IPMI device '%s'.", ipmi_dev);
		close(ipmi_fd);
		free(ipmi_dev);
		ipmi_dev = NULL;
		ipmi_fd = -1;
	}
	is_open = false;
}

// If msg queue is full on send or empty on read, wait ms milliseconds and try
// again. BMC stuff is not thread-safe, so one sleep_time for send & recv is ok.
#define WAIT_TIME_IN_MS 1
static const struct timespec sleep_time =
	{ .tv_sec = 0, .tv_nsec = WAIT_TIME_IN_MS * 1000000 };

int
ipmi_send(struct ipmi_rq *req) {
	struct strbuf sb;
	int res = 0, maxtries;

	static uint32_t curr_seq = 0;

	if (req == NULL)
		return -1;
	if (! (is_open && ipmi_fd >= 0)) {
		PROM_FATAL("IPMI device not opened.", "");
		return -2;
	}

#ifdef DEBUG_IPMI_IF
	PROM_DEBUG("ipmi req: netfn = 0x%02x  cmd = 0x%02x  dlen = %d",
		req->msg.netfn, req->msg.cmd, req->msg.data_len);
	if (req->msg.data_len > 0)
		PROM_DEBUG("Raw request data:\n%s",
			hexdump(req->msg.data, req->msg.data_len, 1));
#endif

	int msgsz = offsetof(bmc_msg_t, msg) + sizeof(bmc_req_t);
	if (req->msg.data_len > SEND_MAX_PAYLOAD_SIZE)
		msgsz += (req->msg.data_len - SEND_MAX_PAYLOAD_SIZE);
	bmc_msg_t *msg = malloc(msgsz);
	bmc_req_t *_req = (bmc_req_t *)&msg->msg[0];

	msg->m_type = BMC_MSG_REQUEST;
	msg->m_id = curr_seq++;
	_req->fn = req->msg.netfn;
	_req->lun = 0;
	_req->cmd = req->msg.cmd;
	_req->datalength = req->msg.data_len;
	memcpy(_req->data, req->msg.data, req->msg.data_len);
		
	sb.len = msgsz;
	sb.buf = (char *)msg;
	maxtries = 2 * 1000 / WAIT_TIME_IN_MS;	// wait max. 2s

	while (maxtries > 0) {
		if (putmsg(ipmi_fd, NULL, &sb, 0) < 0) {
			if ((errno == EAGAIN) && (maxtries > 0)) {
#ifdef DEBUG_IPMI_IF
				PROM_DEBUG("Message queue full - sleeping %d ms.",
					WAIT_TIME_IN_MS);
#endif
				nanosleep(&sleep_time, NULL);
				maxtries--;
				continue;
			}
			char *str = strerror(errno);
			PROM_WARN("Failed to send request %d (fn=0x%02x cmd=0x%02x): %s",
				msg->m_id, req->msg.netfn, req->msg.cmd, str);
			res = -3;
		} else {
			res = msg->m_id;
#ifdef DEBUG_IPMI_IF
			PROM_DEBUG("done. msgId: %d", res);
#endif
		}
		break;
	}

	// cleanup
	free(msg);
	msg = NULL;

	return res;
}


struct ipmi_rs *
ipmi_recv(long msgid, long timeout) {
	bmc_msg_t *msg;
	int flags = 0, maxtries, rem;

	static struct strbuf sb;
	static struct ipmi_rs rsp;
	static char data[sizeof(rsp.data)];

	if (! (is_open && ipmi_fd >= 0)) {
		PROM_FATAL("IPMI device not opened.", "");
		return NULL;
	}

	memset(&rsp, 0, sizeof (struct ipmi_rs));
	sb.buf = data;
	msg = (bmc_msg_t *)sb.buf;
	sb.maxlen = sizeof(data);

	maxtries = ((timeout <= 0) ? 5: timeout) * 1000 / WAIT_TIME_IN_MS;
	rem = maxtries;

again:
	msg->m_type = 0;
	while (maxtries > 0) {
		if (getmsg(ipmi_fd, NULL, &sb, &flags) >= 0)
			break;
		if ((errno == EAGAIN) && (maxtries > 0)) {
			nanosleep(&sleep_time, NULL);
			maxtries--;
			continue;
		}
		char *str = strerror(errno);
		PROM_WARN("Fetching data for request %d failed: %s", msgid, str);
		return NULL;
	}
	if (sdr_verbose > 1)
		PROM_DEBUG("Slept %d times for %d ms", rem - maxtries, WAIT_TIME_IN_MS);

	bmc_rsp_t *bmc_res;
	if (msg->m_type == BMC_MSG_ERROR) {
		char *str = strerror(msg->msg[0]);
		PROM_WARN("Error for request %d: %s", msgid, str);
		return NULL;
	} else if (msg->m_type != BMC_MSG_RESPONSE) {
		PROM_WARN("Unexpected msg type 0x%02x - message %d ignored.",
			msg->m_type, msg->m_id);
		return NULL;
	}
	if (msgid != msg->m_id) {
		PROM_WARN("Oooops, fetched an unexpected message: %d != %d - %s.",
			msg->m_id, msgid, (maxtries > 0) ? "retrying" : "giving up");
		if (maxtries > 0)
			goto again;
		else
			return NULL;
	}

	// de-couple response from the running OS
	bmc_res = (bmc_rsp_t *)&msg->msg[0];
#ifdef DEBUG_IPMI_IF
	PROM_DEBUG("Raw response (1 + %d bytes):\n%s\n", bmc_res->datalength,
		hexdump((const uint8_t *) bmc_res->data, bmc_res->datalength, 1));
#endif
	rsp.ccode = bmc_res->ccode;
	rsp.data_len = bmc_res->datalength;
	if (rsp.data_len > 0)
		memcpy(rsp.data, bmc_res->data, rsp.data_len);

	return &rsp;
}
