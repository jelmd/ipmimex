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
 * @file openipmi.c
 * OpenIPMI interface for Linux driven boxes.
 */
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>

#include <linux/ipmi.h>			// linux-headers.deb

#include "common.h"
#include "ipmi_if.h"

#ifdef DEBUG_IPMI_IF
#include "hexdump.h"
#endif

static bool is_open = false;
static char *ipmi_dev = NULL;
static int ipmi_fd = -1;

int
ipmi_if_open(char *dev) {
	if (is_open) {
		PROM_WARN("IPMI device '%s' already open.", ipmi_dev);
		return 0;
	}
	ipmi_dev = (dev == NULL) ? strdup("/dev/ipmi0") : strdup(dev);
	PROM_INFO("Using OpenIPMI device '%s' ...", ipmi_dev);
	ipmi_fd = open(ipmi_dev, O_RDWR);
	if (ipmi_fd < 0) {
		PROM_FATAL("Unable to open '%s' in RW mode.", ipmi_dev);
		free(ipmi_dev);
		ipmi_dev = NULL;
		return 1;
	}
	is_open = true;

	unsigned int val = false;
	if (ioctl(ipmi_fd, IPMICTL_SET_GETS_EVENTS_CMD, &val) < 0) 
		PROM_WARN("Could not explicitly disable event receiver", "");

	val = IPMI_BMC_SLAVE_ADDR;
	if (ioctl(ipmi_fd, IPMICTL_SET_MY_ADDRESS_CMD, &val) < 0) {
		PROM_FATAL("Unable to set my_addr to '0x%02x'.", val);
		ipmi_if_close();
		return 2;
	}

	return 0;
}

void
ipmi_if_close(void) {
	if (is_open && ipmi_fd >= 0) {
		PROM_INFO("Closing IPMI device '%s'.", ipmi_dev);
		close(ipmi_fd);
		free(ipmi_dev);
		ipmi_dev = NULL;
		ipmi_fd = -1;
	}
	is_open = false;
}

static struct ipmi_system_interface_addr bmc_addr = {
	.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
	.channel = IPMI_BMC_CHANNEL,
	.lun = 0
};

int
ipmi_send(struct ipmi_rq *req) {
	struct ipmi_req _req;

	static int curr_seq = 0;

	if (req == NULL)
		return -1;

	if (! (is_open && ipmi_fd >= 0)) {
		PROM_WARN("IPMI device not opened.", "");
		return -2;
	}

#ifdef DEBUG_IPMI_IF
	PROM_DEBUG("ipmi req: fn = 0x%02x  cmd = 0x%02x  dlen = %d",
		req->msg.netfn, req->msg.cmd, req->msg.data_len);
	if (req->msg.data_len > 0)
		PROM_DEBUG("Raw request data:\n%s",
			hexdump(req->msg.data, req->msg.data_len, 1));
#endif

	memset(&_req, 0, sizeof(struct ipmi_req));

	_req.addr = (unsigned char *)&bmc_addr;
	_req.addr_len = sizeof(bmc_addr);
	_req.msgid = curr_seq++;
	if (curr_seq < 0)
		curr_seq = 0;
	_req.msg.data = req->msg.data;
	_req.msg.data_len = req->msg.data_len;
	_req.msg.netfn = req->msg.netfn;
	_req.msg.cmd = req->msg.cmd;

	if (ioctl(ipmi_fd, IPMICTL_SEND_COMMAND, &_req) < 0) {
		char *str = strerror(errno);
		PROM_WARN("Failed to send ipmi request %d (fn=0x%02x cmd=0x%02x): %s",
			_req.msgid, req->msg.netfn, req->msg.cmd, str);
		return -3;
	}
#ifdef DEBUG_IPMI_IF
	PROM_DEBUG("done. msgId: %d", _req.msgid);
#endif

	return _req.msgid;
}

struct ipmi_rs *
ipmi_recv(long msgid, long timeout)  {
	fd_set rfds;				// fd set to monitor
	struct timeval tv;			// max time to wait
	struct ipmi_addr addr;
	struct ipmi_recv recv;
	int res;

	static struct ipmi_rs rsp;
	
	if (! (is_open && ipmi_fd >= 0)) {
		PROM_FATAL("IPMI device not opened.", "");
		return NULL;
	}
	memset(&rsp, 0, sizeof (struct ipmi_rs));

	FD_ZERO(&rfds);				// clear the set
	FD_SET(ipmi_fd, &rfds);		// add ipmi_fd to the set
	tv.tv_sec = timeout <= 0 ? 5 : timeout;
	tv.tv_usec = 0;
	recv.msgid = -1;

	while (msgid != recv.msgid) {
		// wait 'til ready to read/timeout but ignore interrupts
		do {
			res = select(ipmi_fd + 1, &rfds, NULL, NULL, &tv);
		} while (res < 0 && errno == EINTR);
		// any data available ?
		if (res < 0) {
			char *str = strerror(errno);
			PROM_WARN("Error for request %d: %s", str);
			return NULL;
		} else if (res == 0) {
			PROM_WARN("Timeout for request %d.", msgid);
			return NULL;
		}
		// get the data
		recv.addr = (unsigned char *)&addr;
		recv.addr_len = sizeof(addr);
		recv.msg.data = rsp.data;
		recv.msg.data_len = sizeof(rsp.data);
		if (ioctl(ipmi_fd, IPMICTL_RECEIVE_MSG, &recv) < 0) {
			char *str = strerror(errno);
			PROM_WARN("Fetching data for request %d failed: %s", msgid, str);
			// Actually this should not happen, because our buffer size is 1024
			// bytes. Max. payload is limited by uint8 256 and any command
			// specific data at the head of the payload are max. 32 bytes, even
			// for OEM records.
			if ((res == EMSGSIZE) && (msgid == recv.msgid))
				break;
			return NULL;
		}
		if (msgid != recv.msgid) {
			PROM_WARN("Oooops, fetched an unexpected message: %d != %d",
				recv.msgid, msgid);
		}
	}

	// de-couple response from the running OS
#ifdef DEBUG_IPMI_IF
	PROM_DEBUG("Raw response (1 + %d bytes):\n%s\n", recv.msg.data_len - 1,
		hexdump(recv.msg.data + 1, recv.msg.data_len - 1, 1));
#endif
	rsp.ccode = recv.msg.data[0];
	rsp.data_len = recv.msg.data_len - 1;	// 1st byte is the completion code
	if (rsp.data_len > 0)
		memmove(rsp.data, recv.msg.data + 1, rsp.data_len);

	return &rsp;
}
