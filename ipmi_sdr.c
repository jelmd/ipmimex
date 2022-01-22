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

#include <prom_string_builder.h>
#include <prom_log.h>

#include "ipmi_if.h"
#include "ipmi_sdr.h"
#include "ipmi_sdr_convert.h"
#include "hexdump.h"

// SDR NetFn, table 5-1
#define NETFN_SE		0x4			// Sensor|Event cmd
#define NETFN_APP		0x6			// Application cmd
#define NETFN_STORAGE	0xA			// Non-volatile storage Requests
#define NETFN_DCGRP		0x2C		// Group Extension

#define CMD(_a, _b, _c, _d)	\
	static struct ipmi_rq _a; \
	_a.msg.cmd = _b; \
	_a.msg.netfn = _c; \
	_a.msg.lun = 0; \
	_a.msg.data_len = 0; \
	_a.msg.data = NULL; \
	if (_d) \
		*(_d) = 0xFF;				// unspecified

// SDR commands, table G-1
#define CMD_GET_DEV_ID(m,r)				CMD(m, 0x01,NETFN_APP, r)		// 20.1
#define CMD_GET_POWER_READING(m,r)		CMD(m, 0x02,NETFN_DCGRP, r)// DCMI 6.6.1
#define CMD_GET_SDR_INFO(m,r)			CMD(m, 0x20,NETFN_STORAGE, r)	// 33.9
#define CMD_GET_RESERVATION_ID(m,r)		CMD(m, 0x22,NETFN_STORAGE, r)	// 33.11
#define CMD_GET_SDR(m,r)				CMD(m, 0x23,NETFN_STORAGE, r)	// 33.12
#define CMD_GET_SENSOR_FACTORS(m,r)		CMD(m, 0x23,NETFN_SE, r)		// 35.5
#define CMD_GET_SENSOR_THRESHOLD(m,r)	CMD(m, 0x27,NETFN_SE, r)		// 35.9
#define CMD_GET_SENSOR_READING(m,r)		CMD(m, 0x2D,NETFN_SE, r)		// 35.14

#define SEND(_a, _b, _c, _d, ...)	\
	if ((_b = ipmi_send(&_a)) < 0) { \
		if (_b == -3) \
			PROM_WARN(_d, __VA_ARGS__); \
		return _c; \
	}

#define RECV(_a, _b, _c, _d, _e, ...)	\
	struct ipmi_rs *_a = ipmi_recv(_b, 0); \
	if (_a == NULL) { \
		PROM_WARN(_e, __VA_ARGS__); \
		return _c; \
	} \
	if (_d) \
		*(_d) = _a->ccode;

ipmi_bmc_info_t *
get_bmc_info(uint8_t *cc) {
	int msgId;
	static ipmi_bmc_info_t bmc_info;

	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting BMC info.", "");
	CMD_GET_DEV_ID(req, cc);

	if (cc)
		*cc = 0xFF;
	SEND(req, msgId, NULL, "Failed to send BMC info request.", "");
	RECV(rsp, msgId, NULL, cc, "Unable to get BMC info.", "");

	if (rsp->ccode != 0) {
		PROM_WARN("BMC info request failed with: %s", ipmi_cc2str(rsp->ccode));
		return NULL;
	}
	if (bmc_info.update_in_progress) {
		*cc = SDR_CC_FW_UPDATE_IN_PROGRESS;
		return NULL;
	}
	memcpy(&bmc_info, rsp->data, sizeof (bmc_info));

	PROM_DEBUG("BMC %s Device SDRs",
		bmc_info.provides_dev_sdrs ? "provides" : "does not provide");
	PROM_DEBUG("BMC %s SDR repo device commands",
		bmc_info.supports_sdr_repo ? "supports" : "does not support");
	PROM_DEBUG("BMC %s SDR sensor device commands",
		bmc_info.supports_sensor ? "supports" : "does not support");

	return &bmc_info;
}

sdr_repo_info_t *
get_repo_info(uint8_t *cc) {
	int msgId;

	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting repo info.", "");
	// This implementation is not interested in satellite MCs/Devs alias 
	// Device SDRs, but in the BMC managed repo (LUN 0), only.
	CMD_GET_SDR_INFO(req, cc);
	SEND(req, msgId, NULL, "Failed to send SDR repo info request.", "");
	RECV(rsp, msgId, NULL, cc, "Failed to get SDR repo info.", "");
	if (rsp->ccode != 0) {
		PROM_WARN("Get SDR repository info command failed with: %s",
			ipmi_cc2str(rsp->ccode));
		return NULL;
	}

	static sdr_repo_info_t sdr_info;
	memcpy(&sdr_info, rsp->data, sizeof (sdr_info));
	// IPMIv1.0 == 0x01; IPMIv1.5 == 0x51 ; IPMIv2.0 == 0x02
	if ((sdr_info.version != 0x51) && (sdr_info.version != 0x01)
			&& (sdr_info.version != 0x02))
	{
		PROM_WARN("Unknown SDR repository version 0x%02x", sdr_info.version);
	}
	PROM_DEBUG("SDR records   : %d", sdr_info.sdr_count);
	return &sdr_info;
}

uint16_t
get_reservation(uint8_t *cc) {
	int msgId;
	
	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting repo reservation", "");
	CMD_GET_RESERVATION_ID(req, cc);
	SEND(req, msgId, 0, "Failed to send reservation request.", "")
	RECV(rsp, msgId, 0, cc, "Get reservation request failed.", "");
	PROM_DEBUG("New reservation ID: %04x",((sdr_reservation_t *)rsp->data)->id);

	return ((sdr_reservation_t *) rsp->data)->id;
}

/* keep track of reservation ID to use for GET SDR commands. */
static uint16_t reservation_id = 0;

sdr_full_t *
get_sdr(uint16_t *record_id, uint8_t *len, uint8_t *cc) {
	int msgId;
	uint16_t rid = *record_id, res_count_try = 0;

	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting SDR 0x%04x", *record_id);
	if (record_id == NULL || len == NULL) {
		PROM_FATAL("Software bug: recordId & len must be != NULL", "");
		return NULL;
	}
	sdr_reservation_t sdr_reserv;
	sdr_reserv.record_id = *record_id;
	sdr_reserv.offset = 0;
	sdr_reserv.len = *len;

	CMD_GET_SDR(req, cc);

	req.msg.data = (uint8_t *) &sdr_reserv;
	req.msg.data_len = sizeof (sdr_reserv);

again:
	sdr_reserv.id = reservation_id;
	
	*record_id = 0;
	*len = 0;

	SEND(req, msgId, NULL, "Failed to send get request for SDR 0x%04x.", rid);
	RECV(rsp, msgId, NULL, cc, "Failed to get SDR 0x%04x.", rid);

	sdr_full_t *sdr = NULL;
	if (rsp->data_len > 1) {
		*len = rsp->data_len - 2;
		// let's '\0' terminate so that name can be printed w/o copying before
		(rsp->data)[rsp->data_len] = 0;
		sdr = (sdr_full_t *) (rsp->data + 2);
		if (*len >= 5) {
			if (rid != sdr->id && rid != 0) {
				PROM_WARN("ID of the SDR obtained is != requested ID."
					"(0x%04x != 0x%04x). Adjusting SDR ID.", sdr->id, rid);
				sdr->id = rid;
			}
		}
		*record_id = *((uint16_t *) rsp->data);
	} else {
		msgId = 0;
	}

	if (rsp->ccode != 0) {
#define SDR_MSG "Get SDR command failed with: %s"
		if (rsp->ccode == SDR_CC_RESERVATION_CANCELED) {
			PROM_DEBUG(SDR_MSG, ipmi_cc2str(rsp->ccode));
			if (res_count_try > 0 && res_count_try < 4)
				sleep(1);
			if (res_count_try < 4) {
				reservation_id = get_reservation(cc);
				res_count_try++;
				goto again;
			}
		} else {
			PROM_WARN(SDR_MSG, ipmi_cc2str(rsp->ccode));
		}
#undef SDR_MSG
		if (rsp->ccode == SDR_CC_BUFFER_TOO_SMALL) {
			PROM_WARN("Very unusual today. Please report via %s", ISSUES_URL);
			// keep and deliver partial message
		} else {
			return NULL;
		}
	}
	if (msgId == 0) {
		// just in case the ccode check did not catch it
		PROM_WARN("Got invalid response for SDR 0x%04 request.", rid);
		return NULL;
	}

	if (ipmi_verbose) {
		const char *s =
			(ipmi_verbose > 1) ? hexdump((uint8_t *)sdr,*len + 1,1) : "";
		if (*len > 48) {
			PROM_DEBUG("\nGot SDR 0x%04x for sensor 0x%02x:\n"
				"\tsize: %ld/%ld\n\ttype: 0x%02x\n"
				"\tname: '%s', Len: %d, Fmt: %d\n%s",
				rid, sdr->keys.sensor_num, *len, sdr->size + 5, sdr->type,
				sdr->name.raw, sdr->name.len, sdr->name.fmt, s);
		} else {
			PROM_DEBUG("\nGot SDR 0x%04x for sensor 0x%02x (%ld bytes)\n%s",
				rid, sdr->keys.sensor_num, *len, s);
		}
	}
	return sdr;
}

sdr_thresholds_t *
get_thresholds(uint8_t snum, uint8_t *cc) {
	int msgId;
	
	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting thresholds for sensor 0x%02x", snum);
	CMD_GET_SENSOR_THRESHOLD(req, cc);
	req.msg.data = &snum;
	req.msg.data_len = sizeof(snum);
	
	SEND(req,msgId,NULL,"Failed to send thresholds cmd for sensor 0x%02x",snum);
	RECV(rsp,msgId,NULL,cc,"Failed to get thresholds for sensor 0x%02x.", snum);

	if (rsp->ccode != 0) {
		// DELL likes to screw up its BMCs (junkware) ... 
		if ((rsp->ccode != SDR_CC_SENSOR_NOT_FOUND)
			&& (rsp->ccode != SDR_CC_ILLEGAL_CMD))
		{
			PROM_WARN("Get thresholds for sensor 0x%02x failed with: %s",
				snum, ipmi_cc2str(rsp->ccode));
		}
		return NULL;
	}
	if (rsp->data_len != sizeof(sdr_thresholds_t)) {
		PROM_WARN("Got invalid thresholds for sensor 0x%02x.", snum);
		return NULL;
	}

	return (sdr_thresholds_t *) rsp->data;
}

sdr_reading_t *
get_reading(uint8_t snum, char *name, uint8_t *cc) {
	int msgId;

	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting value for sensor 0x%02x", snum);
	CMD_GET_SENSOR_READING(req, cc);
	req.msg.data = &snum;
	req.msg.data_len = sizeof(snum);
	SEND(req,msgId,NULL,"Failed to send get value cmd for sensor 0x%02x (%s).",
		snum, name);
	RECV(rsp, msgId, NULL, cc, "Failed to get value for sensor 0x%02x (%s).",
		snum, name);

	if (rsp->ccode != 0) {
		if (rsp->ccode == SDR_CC_SENSOR_NOT_FOUND) {
			PROM_DEBUG("Sensor '%s' not found.", name);
		} else if (rsp->ccode == SDR_CC_CMD_TMP_UNSUPPORTED) {
			PROM_DEBUG("Sensor '%s' reading currently not supported.", name);
		} else {
			PROM_WARN("Reading value of sensor 0x%02x (%s) failed with: %s",
				snum, name, ipmi_cc2str(rsp->ccode));
		}
		return NULL;
	}
	if (rsp->data_len < (int) sizeof(sdr_reading_t)) {
		// Well, the 4th byte is optional for non-discrete sensors.
		if (rsp->data_len == sizeof(sdr_reading_t) - 1) {
			((sdr_reading_t *) rsp->data)->state1 = 0;
		} else {
			// Gigabyte likes to send bogus answers for unconnected devices
			if (((sdr_reading_t *) rsp->data)->unavailable) {
				*cc = SDR_CC_SENSOR_NOT_FOUND;
				return NULL;
			}
			PROM_WARN("Reading the value of sensor 0x%02x (%s) failed"
				" - too short",
				snum, name, rsp->data_len, (int) sizeof(sdr_reading_t));
			if (ipmi_verbose > 1) {
				PROM_DEBUG("response (%d bytes):\n%s",
					rsp->data_len, hexdump(rsp->data, rsp->data_len, 1));
			}
			return NULL;
		}
	}

	return (sdr_reading_t *) rsp->data;
}

sdr_factors_t *
get_factors(uint8_t snum, uint8_t reading, uint8_t *cc) {
	int msgId;
	uint8_t data[2] = { snum, reading };

	if (ipmi_verbose > 1)
		PROM_DEBUG("Getting value for sensor 0x%02x", snum);
	CMD_GET_SENSOR_FACTORS(req, cc);
	req.msg.data = data;
	req.msg.data_len = sizeof(data);
	SEND(req, msgId, NULL,
		"Failed to send get factors cmd for sensor 0x%02x", snum);
	RECV(rsp, msgId, NULL, cc, "Failed to get factors for sensor 0x%02x.",snum);

	if (rsp->ccode != 0) {
		if (rsp->ccode != SDR_CC_SENSOR_NOT_FOUND) {
			PROM_WARN("Reading value of sensor 0x%02x failed with: %s",
				snum, ipmi_cc2str(rsp->ccode));
		}
		return NULL;
	}
	if (rsp->data_len < (int) sizeof(sdr_factors_t)) {
		PROM_WARN("Reading factors of sensor 0x%02x failed - too short",snum);
		return NULL;
	}

	return (sdr_factors_t *) rsp->data;
}

sdr_power_t *
get_power(uint8_t *cc) {
	uint8_t msg_data[4];
	int msgId;

	msg_data[0] = 0xDC;		// Group Extension Identification: DCMI Spec
	msg_data[1] = 0x01;		// Mode: System Power Statistics
							// 0x02 is worthless for us and often does not work.
	msg_data[2] = 0x00;		// reserved for mode attrs
	msg_data[3] = 0x00;		// reserved

	CMD_GET_POWER_READING(req, cc);
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	SEND(req, msgId, NULL, "Failed to send power reading request.", "");
	RECV(rsp, msgId, NULL, cc, "Failed to get power reading.", "");

	if (rsp->ccode != 0) {
		if (rsp->ccode == SDR_CC_INVALID_CMD) {
			PROM_INFO("DCMI power reading is not supported by this BMC.", "");
		} else {
			PROM_WARN("Power reading request failed with: %s (0x%02x)",
				ipmi_cc2str(rsp->ccode), rsp->ccode);
		}
		return NULL;
	}

	return (sdr_power_t *) rsp->data;
}

void
free_sensor(sensor_t *sensor) {
	sensor_t *scurr = sensor, *rem;

	while (scurr != NULL) {
		PROM_DEBUG("Freeing sensor '%s'", scurr->name);
		rem = scurr->next;
		scurr->next = NULL;
		free(scurr->name);
		free(scurr->factors);
		free(scurr->it_unit);
		free(scurr->it_thresholds);
		free(scurr->prom.name);
		free(scurr->prom.unit);
		free(scurr->prom.mname_reading);
		free(scurr->prom.mname_threshold);
		free(scurr->prom.mname_state);
		free(scurr->prom.note);
		free(scurr);
		scurr = rem;
	}
}

#define IPMIT_NAME_FMT				"%-16s "
#define IPMIT_ANALOG_STATE_FMT		"| %-6s"
#define IPMIT_ANALOG_FMT			"| %-10.3f"
#define IPMIT_DISCRETE_FMT			"| 0x%-8x"
#define IPMIT_DISCRETE_STATE_FMT	"| 0x%02x%02x"
#define IPMIT_NA_FMT				"| %-10s"
#define IPMIT_TFMT \
	IPMIT_NA_FMT IPMIT_NA_FMT IPMIT_NA_FMT \
	IPMIT_NA_FMT IPMIT_NA_FMT IPMIT_NA_FMT

char *
thresholds2ipmitool_str(sdr_thresholds_t *t, uint8_t analog_fmt, factors_t *f) {
	// name | value | unit | threshold_status  |lnr|lcr|lnc|unc|ucr|unr
	//                                         ^^^^^^^^^^^^^^^^^^^^^^^^
	uint8_t offset = 0, n;	// avoids buf overflow
	char buf[256];			// actually 82 chars should be sufficient

	if (t == NULL || t->readable.value == 0) {
		snprintf(buf, sizeof(buf), IPMIT_TFMT,"na","na","na","na","na","na");
		return strdup(buf);
	}

#define TADD(_a)  \
	if (!t->readable._a) { \
		n = sprintf(buf + offset, IPMIT_NA_FMT , "na"); \
	} else if (SDR_UNIT_FMT_IS_DISCRETE(analog_fmt)) { \
		n = sprintf(buf + offset, IPMIT_DISCRETE_FMT , t->_a); \
	} else { \
		n = sprintf(buf + offset, IPMIT_ANALOG_FMT , \
			sdr_convert_value(t->_a , analog_fmt, f)); \
	} \
	offset += n;

	buf[0] = '\0';
	TADD(lower_nr);
	TADD(lower_cr);
	TADD(lower_nc);
	TADD(upper_nc);
	TADD(upper_cr);
	TADD(upper_nr);
	buf[offset] = '\0';

#undef TADD

	return strdup(buf);
}

sensor_t *
scan_sdr_repo(uint32_t *count, bool ignore_disabled, bool drop_noread,
	uint8_t *cc)
{
	sdr_full_t *sdr;
	char *sname;

	uint8_t len = 0;
	uint16_t recId = 0, scanned = 0;
	sdr_repo_info_t *repo_info = get_repo_info(cc);
	sensor_t *slist = NULL, *slast = NULL, *snew;

	*count = 0;
	if (repo_info == NULL || *cc != 0)
		return NULL;
	if (repo_info->sdr_count == 0) {
		PROM_WARN("SDR repository contains no SDRs.", "");
		return NULL;
	}

	while (recId != 0xFFFF) {
		len = 0xFF;
		sdr = get_sdr(&recId, &len, cc);
		scanned++;
		if (*cc != 0)
			return slist;
		if (sdr == NULL || len < 6)
			continue;
		if (len < 48 || sdr->type != SDR_TYPE_FULL_SENSOR) {
			PROM_DEBUG("SDR 0x%04x ignored (type 0x%02x).", sdr->id, sdr->type);
			continue;
		}
		sname = sdr_str2utf8(sdr->name.raw, sdr->name.len, sdr->name.fmt);
		// check common properties
		if (!SDR_IS_THRESHOLD_BASED(sdr->evt_type)) {
			PROM_DEBUG("Non-threshold SDR of sensor '%s' (0x%02x) ignored.",
				sdr->name.raw, sdr->keys.sensor_num);
			free(sname);
			continue;
		}
		if (SDR_UNIT_FMT_IS_DISCRETE(sdr->unit.analog_fmt)) {
			// Paranoid? Actually evt_type check above should have kicked it.
			PROM_DEBUG("Discrete unit SDR '%s' (0x%02x) ignored.",
				sdr->name.raw, sdr->keys.sensor_num);
			free(sname);
			continue;
		}
		if (sdr->disabled) {
			if (ignore_disabled) {
				PROM_INFO("Ignoring 'disabled' flag of sensor '%s' (0x%02x).",
					sdr->name.raw, sdr->keys.sensor_num);
			} else {
				PROM_INFO("Dropping sensor '%s' (0x%02x): disabled",
					sdr->name.raw, sdr->keys.sensor_num);
				free(sname);
				continue;
			}
		}

		snew = (sensor_t *) malloc(sizeof(sensor_t));
	   	if (snew == NULL) {
			PROM_FATAL("Unable to allocate a sensor entry.", "");
			*cc = SDR_CC_OUT_OF_SPACE;
			free(sname);
			return slist;
		}
		memset(snew, 0, sizeof(sensor_t));
		if (slist == NULL)
			slist = snew;
		snew->name = sname;
		snew->record_id = sdr->id;
		snew->owner_id = sdr->keys.owner_id;
		snew->owner_lun = sdr->keys.owner_lun;
		snew->sensor_num = sdr->keys.sensor_num;
		snew->unit = sdr->unit;
		snew->category = sdr->category;
		snew->it_unit = strdup(sdr_unit2str(&(sdr->unit)));
		// Wondering, who has ever seen it ...
		if (SDR_LTYPE_IS_NON_LINEAR(sdr->factors.linearization)) {
			PROM_WARN("Slow sensor '%s' (SDR %d) found.", snew->name, sdr->id);
		} else {
			snew->factors = sdr_factors2factors(&(sdr->factors));
		}

		get_reading(snew->sensor_num, snew->name, cc);
		if (*cc == SDR_CC_SENSOR_NOT_FOUND) {
			PROM_INFO("Dropping sensor '%s' (0x%02x): probably "
				"not populated/connected.", snew->name, snew->sensor_num);
			free_sensor(snew);
			if (snew == slist)
				slist = NULL;
			continue;
		}
		if (drop_noread && *cc == SDR_CC_CMD_TMP_UNSUPPORTED) {
			PROM_INFO("Dropping sensor '%s' (0x%02x): no read.",
				snew->name, snew->sensor_num);
			free_sensor(snew);
			if (snew == slist)
				slist = NULL;
			continue;
		}
		if (*cc != 0 && *cc != SDR_CC_CMD_TMP_UNSUPPORTED)
			return slist;

		if (slast != NULL)
			slast->next = snew;
		slast = snew;
		(*count)++;
	}
	PROM_DEBUG("Found %d of %d scanned SDRs eligible.", *count, scanned);
	*cc = 0;

	return slist;
}

bool
sdrs_changed(sensor_t *head) {
	sensor_t *s = head;
	sdr_full_t *sdr;
	uint8_t cc = 0, len = 8;
	uint16_t rid;
	static uint32_t last_add = 0xFFFFFFFE, last_del = 0xFFFFFFFE, ladd, ldel;

	sdr_repo_info_t *ri = get_repo_info(&cc);

	if (ri == NULL)
		return false;	// can't say anything, so assume a temp error

	PROM_DEBUG("Repo: last add: %d/%d   last del: %d/%d",
		last_add, ri->last_add, last_del, ri->last_del);

	if (cc == 0 && head == NULL)
		return true;
	if (ri != NULL && last_add == ri->last_add && last_del == ri->last_del)
		return false;
	ladd = ri->last_add;
	ldel = ri->last_del;

	while (s != NULL) {
		rid = s->record_id;
		sdr = get_sdr(&rid, &len, &cc);
		if (sdr == NULL)
			return true;
		if (s->owner_id != sdr->keys.owner_id
			|| s->owner_lun != sdr->keys.owner_lun
			|| s->sensor_num != sdr->keys.sensor_num)
		{
			return true;
		}
		s = s->next;
	}

	last_add = ladd;
	last_del = ldel;
	return  false;
}

void
show_ipmitool_sensors(sensor_t *list, psb_t *sb, bool extended) {
	sdr_reading_t *r;
	sdr_factors_t *f;
	factors_t *rf;
	uint8_t value, cc, tstate;
	double real_val;
	sensor_t *s = list;
	bool free_sb = sb == NULL;
	char buf[512];

	if (s == NULL)
		return;

	// remember sb state
	if (free_sb)
		sb = psb_new();

	if (extended)
		psb_add_str(sb, " SDR  |sensor|");
	sprintf(buf,
		IPMIT_NAME_FMT IPMIT_NA_FMT " " IPMIT_NA_FMT " " IPMIT_ANALOG_STATE_FMT
		IPMIT_TFMT,
		"Name", "Value", "Unit", "State", 
		"lower_nr","lower_cr","lower_nc","upper_nc","upper_cr","upper_nr");
	psb_add_str(sb, buf);
	if (extended)
		psb_add_str(sb, "| T-State");
	psb_add_str(sb, "\n");

	while (s != NULL) {
		r = get_reading(s->sensor_num, s->name, &cc);
		if (r == NULL) {
			PROM_DEBUG("No reading for sensor '%s' (%d).",
				s->name, s->sensor_num);
			goto next;
		}
		if (r->unavailable || !r->scanning_enabled) {
			PROM_DEBUG("Reading for sensor '%s' (%d).",
				s->name, s->sensor_num,
				r->unavailable ? "unavailable" : "disabled");
			goto next;
		}
		value = r->value;
		tstate = r->state0 & 0x3F;
		if (SDR_LTYPE_IS_NON_LINEAR(s->factors->linearization)) {
			f = get_factors(s->sensor_num, value, &cc);
			if (f == NULL)
				goto next;
			rf = sdr_factors2factors(f);
			if (rf == NULL)
				goto next;
		} else {
			rf = s->factors;
		}
		real_val = sdr_convert_value(value, s->unit.analog_fmt, rf);
		if (extended) {
			sprintf(buf, " %04x |  %02x  |", s->record_id, s->sensor_num);
			psb_add_str(sb, buf);
		}
		// since we do not support discrete values, state is always 'ok'.
		sprintf(buf, IPMIT_NAME_FMT IPMIT_ANALOG_FMT " " IPMIT_NA_FMT " "
			IPMIT_ANALOG_STATE_FMT,
			s->name, real_val, s->it_unit, "ok");
		psb_add_str(sb, buf);

		if (s->it_thresholds == NULL) {
			sdr_thresholds_t *t = get_thresholds(s->sensor_num, &cc);
			if (t == NULL) {
				PROM_INFO("Sensor '%s' (0x%02x) provides no thresholds.",
					s->name, s->sensor_num);
			} else {
				s->it_thresholds =
					thresholds2ipmitool_str(t, s->unit.analog_fmt, s->factors);
			}
		}

		if (s->it_thresholds)
		   psb_add_str(sb, s->it_thresholds);
		if (extended) {
			if (!s->it_thresholds) {
				sprintf(buf, IPMIT_TFMT, "","","", "","","");
				psb_add_str(sb, buf);
			}
			sprintf(buf, "| %02x", tstate);
			psb_add_str(sb, buf);
		}
		psb_add_str(sb, "\n");
next:
		s = s->next;
	}

	sdr_power_t *p = get_power(&cc);
	if (p != NULL) {
		psb_add_str(sb, "\n\n");
		sprintf(buf, "\tInstantaneous power reading: %8d W\n\n", p->curr);
		psb_add_str(sb, buf);
		sprintf(buf, "\tWithin the last %u s:\n", p->sample_time/1000);
		psb_add_str(sb, buf);
		sprintf(buf, "\t\tMin: %8d W\n", p->min);
		psb_add_str(sb, buf);
		sprintf(buf, "\t\tMax: %8d W\n", p->max);
		psb_add_str(sb, buf);
		sprintf(buf, "\t\tAvg: %8d W\n", p->avg);
		psb_add_str(sb, buf);
		sprintf(buf, "\n\tPower reading state is %s.\n",
			((p->state & 0x40) == 0x40) ? "activated" : "deactivated");
		psb_add_str(sb, buf);
	}

	if (free_sb) {
		fprintf(stdout, "%s", psb_str(sb));
		psb_destroy(sb);
	}
}
