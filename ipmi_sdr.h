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

/**
 * @file ipmi_sdr.h
 * IPMI SDR/sensor related functions and definitions.
 */
#ifndef IPMIMEX_IPMI_SDR_H
#define IPMIMEX_IPMI_SDR_H

#include <stdbool.h>
#include <inttypes.h>
#include "mach.h"

#include <prom_string_builder.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int sdr_verbose;

// important command completion codes (kind of temp. errors)
#define SDR_CC_INVALID_CMD				0xC1
#define SDR_CC_INVALID_LUN_CMD			0xC2
#define SDR_CC_TIMEOUT					0xC3
#define SDR_CC_OUT_OF_SPACE				0xC4
#define SDR_CC_RESERVATION_CANCELED		0xC5
#define SDR_CC_BUFFER_TOO_SMALL			0xCA
#define SDR_CC_SENSOR_NOT_FOUND			0xCB
#define SDR_CC_ILLEGAL_CMD				0xCD
#define SDR_CC_REPO_UPDATE_IN_PROGRESS	0xD0
#define SDR_CC_FW_UPDATE_IN_PROGRESS	0xD1
#define SDR_CC_BMC_INIT_IN_PROGRESS		0xD2
#define SDR_CC_DESTINATION_NA			0xD3
#define SDR_CC_CMD_TMP_UNSUPPORTED		0xD5

/** @brief The given command completion code denotes a retryable "soon"
		condition, i.e. \c SDR_CC_*_IN_PROGRESS . */
#define SDR_REPO_TMP_NA(_x) (((_x) & 0xFC) == 0xDC)

#pragma pack(push,1)

/** @brief IPMI v2, table 20-2, Get Device ID Command. (20.1) */
typedef struct ipmi_bmc_info {
	uint8_t id;						// (2)
	BITFIELD3(						// (3)
		provides_dev_sdrs:1,		//	- [7]
		__reserved:3,				//  - [6:4]
		rev:4						//  - [3:0]
	);
	BITFIELD2(						// (4) Firmware Revision 1
		update_in_progress:1,		//	- [7]
		fw_rev_major:7				//	- [6:0]
	);
	uint8_t fw_rev_minor;			// (5)
	uint8_t ipmi_version;			// (6)
	BITFIELD8(						// (7) additional device support
		supports_chassis:1,			//	- [7]
		supports_bridge:1,			//	...
		supports_evtgen:1,
		supports_evtrcv:1,
		supports_fru:1,
		supports_sel:1,
		supports_sdr_repo:1,
		supports_sensor:1
	);
	uint8_t manufacturer_id[3];		// (8:10)
	uint8_t product_id[2];			// (11:12)
	uint8_t aux_fw_rev[4];			// (13:16)
} PACKED ipmi_bmc_info_t;

/** @brief IPMI v2, table 33-3, Get Repository Info Command. (33.9) */
typedef struct sdr_repo_info {
	uint8_t version;				// (2) SDR version (51h == v2.0)
	uint16_t sdr_count;				// (3:4) number of records in the repo
	uint16_t free_bytes;			// (5:6) free space in bytes in the repo
	uint32_t last_add;				// (7:10)  last add timestamp
	uint32_t last_del;				// (11:14) last del timestamp
	uint8_t supported_ops;			// (15) supported operations
} PACKED sdr_repo_info_t;

/** @brief IPMI v2, table 33-6 && 35-3, Get [Device] SDR Command. (33.12 &&
  * 35.4) */
typedef struct sdr_reservation {
	uint16_t id;					// (1:2) reservation ID
	uint16_t record_id;				// (3:4) ID of SDR to get
	uint8_t offset;					// (5) offset into SDR
	uint8_t len;					// (6) # of bytes to get. 0xff for all avail
} PACKED sdr_reservation_t;

/** @brief IPMI v2, table 35-5, Get Sensor Reading Factors Command. (35.5) */
// linearization:
#define SDR_LTYPE_LINEAR     0x00
#define SDR_LTYPE_LN         0x01
#define SDR_LTYPE_LOG10      0x02
#define SDR_LTYPE_LOG2       0x03
#define SDR_LTYPE_E          0x04
#define SDR_LTYPE_EXP10      0x05
#define SDR_LTYPE_EXP2       0x06
#define SDR_LTYPE_1_X        0x07
#define SDR_LTYPE_SQR        0x08
#define SDR_LTYPE_CUBE       0x09
#define SDR_LTYPE_SQRT       0x0a
#define SDR_LTYPE_CUBERT     0x0b
#define SDR_LTYPE_LAST SDR_LTYPE_CUBERT
#define SDR_LTYPE_IS_NON_LINEAR(_v)    (0x70 <= (_v) && (_v) <= 0x7F)
typedef struct sdr_factors {
	union {
		uint8_t next_reading;		// (2)
		struct {
		BITFIELD2(					// SDR full: (24) Linearization
			__reserved2:1,
			linearization:7
		);} PACKED;
	};
	// same as SDR full: (25:30)
	uint8_t M_ls;					// (3) 8 LS bits of M
	BITFIELD2(						// (4)
		M_ms:2,						// 	- [7:6] 2 MS bits of M
		tolerance:6					//	- [5:0] tolerance in ±0.5 ticks
	);
	uint8_t B_ls;					// (5) 8 LS bits of B
	BITFIELD2(						// (6)
		B_ms:2,						//	- [7:6] 2 MS bits of B
		accuracy_ls:6				//	- [5:0] 6 LS bits of Accuracy
	);
	BITFIELD3(						// (7)
		accuracy_ms:4,				//	- [7:4] 4 MS bits of Accuracy
		accuracy_exp:2,				//	- [3:2] Accuracy exponent (unsigned)
		direction:2					//	- [1:0] direction {na,in,out,reserved}
	);
	BITFIELD2(						// (8)
		R:4,						//	- [7:4] Result exponent (signed)
		B:4							//	- [3:0] Base exponent (signed)
	);
} PACKED sdr_factors_t;

/** @brief IPMI v2, table 35-9,Get Sensor Thresholds Command. (35.9) */
typedef struct sdr_thresholds {
	struct {
		union {
			struct {
			BITFIELD7(			// (1) threshold comparison status
				__reserved2:2,	//	- [7:6]
				upper_nr:1,		//	- [5]
				upper_cr:1,		//	- [4]
				upper_nc:1,		//	- [3]
				lower_nr:1,		//	- [2]
				lower_cr:1,		//	- [1]
				lower_nc:1		//	- [0]
			);} PACKED;
			uint8_t value;
		};
	} PACKED readable;
	uint8_t lower_nc;
	uint8_t lower_cr;
	uint8_t lower_nr;
	uint8_t upper_nc;
	uint8_t upper_cr;
	uint8_t upper_nr;
} PACKED sdr_thresholds_t;

/** @brief IPMI v2, table 35-15,Get Sensor Reading Command. (35.14) */
typedef struct sdr_reading {
	uint8_t value;				// (2) Sensor reading if analog_fmt == 1 or 2
	BITFIELD4(					// (3)
		events_enabled:1,		//	- [7] event messages are enabled
		scanning_enabled:1,		//	- [6] sensor scanning enabled
		unavailable:1,			//	- [5] sensor update/init/etc. is running
		__reserved1:5			//	- [4:0]
	);
	union {
		struct {
			BITFIELD7(			// (4) threshold comparison status
				__reserved2:2,	//	- [7:6]
				upper_nr:1,		//	- [5]
				upper_cr:1,		//	- [4]
				upper_nc:1,		//	- [3]
				lower_nr:1,		//	- [2]
				lower_cr:1,		//	- [1]
				lower_nc:1		//	- [0]
			);
		} PACKED threshold;
		uint8_t state0;			// assertion state for discrete sensors
	};
	uint8_t state1;				// (5) assertion state for discrete sensors or 0
} PACKED sdr_reading_t;

/** @brief	IPMI v2, table 43-1, Full Sensor, byte (6:23) is the common part
 * for all SDRs, byte(24:63) the specific part for a full SDR. (43.1) */
typedef struct sdr_full {
	// SENSOR RECORD HEADER (1:5)
	uint16_t id;			// 	(1:2) SDR ID
	uint8_t version;		//	(3) SDR version (51h == 2.0)
	uint8_t type;			//	(4) SDR type - see chapter 43
#	define SDR_TYPE_FULL_SENSOR	0x01
	uint8_t size;			//	(5) SDR size in bytes w/o the header

	// RECORD KEY BYTES (6:8)
	struct {
		union {
			uint8_t owner_id;	// (6) [0] i²c|system addr, [7:1] addr
			struct {
			BITFIELD2(
				addr:7,			//	- [7:1] addr | SW id
				is_id:1			//	- [0] system software ID, otherwise addr
			);} PACKED;
		};
		union {
			uint8_t owner_lun;	// (7) sensor owner lun
			struct {
			BITFIELD3(
				channel:4,		//	- [7:4] channel number
				__reserved1:2,	//	- [3:2]
				lun:2			//	- [1:0] sensor owner lun. 0: SysSW is owner
			);} PACKED;
		};
		uint8_t sensor_num;		// (8) unique sensor number
	} PACKED keys;

	// RECORD BODY BYTES (9:64)
	struct {
		uint8_t id;				// (9) entity code - see table 43-13
		BITFIELD2(				// (10) entity instance
			 is_logical:1,		//	- [7] physical == 0 | logical == 1 entity
			 instance:7			//	- [6:0] instance number
		);
	} PACKED entity;

	BITFIELD8(					// (11) Sensor Initialization
		settable:1,				//	- [7}
		init_scanning:1,		//	- [6]
		init_events:1,			//	- [5]
		init_thresholds:1,		//	- [4]
		init_hysteresis:1,		//	- [3]
		init_type:1,			//	- [2]
		events_enabled:1,		//	- [1]
		scanning_enabled:1		//	- [0]
	);
	BITFIELD5(					// (12) Sensor Capabilities
		disabled:1,				//	- [7]
		auto_rearms:1,			//	- [6]
		hysteresis_support:2,	//	- [5:4] {none,ro,rw,hidden}
		threshold_support:2,	//	- [3:2] {none,ro,rw,hidden} per reading mask
		evt_msg_ctl:2			//	- [1:0] {per thresh ctl,entire,global,no}
	);
	uint8_t category;			// (13) Sensor category - table 42-3 (42.2)
	uint8_t evt_type;			// (14) Event/Reading Type Code - table 42-1
#	define SDR_IS_THRESHOLD_BASED(_v)	((_v) == 0x01)
#	define SDR_IS_GENERIC_DISCRET(_v)	(0x02 <= (_v) && (_v) <= 0x0C)
#	define SDR_IS_SPECIFIC_DISCRET(_v)	(_v == 0x6F)
#	define SDR_IS_OEM_DISCRETE(_v)		(0x70 <= (_v) && (_v) <= 0x7F)

	struct {					// Assertion Event/Threshold Reading Masks
		uint16_t assert;		// (15:16)
		uint16_t deassert;		// (17:18)
		uint16_t discrete;		// (19:20)
	} PACKED mask;

	struct {
		BITFIELD4(				// (21) Sensor Units 1
			analog_fmt:2,		//	- [7:6] analog (numeric) Data Format
			period:3,			//	- [5:3] per {,µs,ms,s,min,h,d,}
			modifier_prefix:2,	//	- [2:1] prefix modifier {'','*','/',''}
			is_percent:1		//	- [0] percentage
		);
#		define SDR_UNIT_FMT_IS_DISCRETE(_v)	(((_v) & 3) == 3)// wrt. analog_fmt
#		define SDR_UNIT_MODIFIER_PREFIX_NONE 0			// ''
#		define SDR_UNIT_MODIFIER_PREFIX_DIV 1			// base / modifier
#		define SDR_UNIT_MODIFIER_PREFIX_MUL 2			// base * modifier
#		define SDR_UNIT_MODIFIER_PREFIX_RSVD 3			// '' (Reserved) 

		uint8_t base;			// (22) Base unit - see table 43-15
		uint8_t modifier;		// (23) Modifier unit. 0 if unused.
	} PACKED unit;

	// END of common SDR data byte(6:23)


	// Full Sensor specific part, byte(24:63)
	sdr_factors_t factors;		// (24:30) - see also 35.5 (2:8)

	struct {
		BITFIELD4(				// (31) Analog characteristic flags
			__reserved3:5,		//	- [7:3]
			normal_min:1,		//	- [2] normal min field specified
			normal_max:1,		//	- [1] normal max field specified
			nominal_read:1		//	- [0] nominal reading field specified
		);
	} PACKED analog_flag;

	uint8_t nominal_reading;	// (32) nominal reading, raw value
	uint8_t normal_max;			// (33) normal maximum, raw value
	uint8_t normal_min;			// (34) normal minimum, raw value
	uint8_t sensor_max;			// (35) sensor maximum, raw value
	uint8_t sensor_min;			// (36) sensor minimum, raw value

	struct {
		struct {
			uint8_t nr;			// (37)	non-recoverable
			uint8_t cr;			// (38) critical
			uint8_t nc;			// (39) non-critical
		} PACKED upper;
		struct {
			uint8_t nr;			// (40)
			uint8_t cr;			// (41)
			uint8_t nc;			// (42)
		} PACKED lower;
		struct {
			uint8_t go_positive;// (43)
			uint8_t go_negative;// (44)
		} PACKED hysteresis;
	} PACKED threshold;

	uint8_t __reserved4[2];		// (45:46)
	uint8_t oem;				// (47) reserved for OEM use

	struct {
		BITFIELD3(				// (48) ID name format and length
			fmt:2,				//	- [7:6] {unicode,BCD+,6b-ASCII,8b-latin1}
			__reserved5:1,		//	- [5]
			len:5				//	- [4:0] raw length in bytes (no trailing \0)
		);
		uint8_t raw[16];		// (49:64) sensor ID string bytes
	} PACKED name;
} PACKED sdr_full_t;


/** @brief Get Power Reading response. DCMI v1.5, table 6-16. (6.6.1) */
typedef struct sdr_power {
	uint8_t grp_xid;		// (1) Group Extension ID
	uint16_t curr;			// (3:4) Current power in W
	uint16_t min;			// (5:6) min. power over sample period in W
	uint16_t max;			// (7:8) max. power over sample period in W
	uint16_t avg;			// (9:10) average power over sampling period in W
	uint32_t timestamp;		// (11:14) IPMI spec based timestamp
	uint32_t sample_time;	// (15:18) sample period in ms for min, max, avg
	uint8_t state;			// (19) Power reading state
							//	- [7] reserved
							//	- [6] power measurement active
							//	- [0:5] reserved
} PACKED sdr_power_t;

#pragma pack(pop)


/**
 * @brief Provides the extracted values from a \c sdr_factors_t struct.
 *	To avoid re-constructing it all the time, they can be cached here.
 */
typedef struct factors {
	int A;							// accuraccy
	int Aexp;						// accuraccy_exp
	int B;
	int Bexp;
	int M;
	int Rexp;
	uint8_t tolerance;				// tolerance in ±0.5 ticks as is
	uint8_t linearization;			// as is
	uint8_t direction;				// as is
} factors_t;


/** @brief Synthetic sensor record */
typedef struct sensor {
	char *name;				// sdr->name.raw converted to latin1
	uint16_t record_id;
	uint8_t owner_id;
	uint8_t owner_lun;
	uint8_t sensor_num;
	uint8_t analog_fmt;
	uint8_t category;		// see full_sensor_t category - table 42-3 (42.2)
	factors_t *factors;		// NULL indicates non-linear: need to fetch factors
							// for each reading.
	char *unit;
	char *it_thresholds;	// ipmitool like formatted thresholds
	struct sensor *next;
} sensor_t;


/**
 * @brief Get Device ID Command.
 * @param cc		If not \c NULL, set to command completion code.
 * @return \c NULL on error, the pointer to the underlying buffer otherwise.
 *	The buffer gets silently overwritten on the next call of this function. 
 * @see	IPMI v2, 20.1
 */
ipmi_bmc_info_t *get_bmc_info(uint8_t *cc);

/**
 * @brief	Get SDR Repository Info Command.
 * @param cc		If not \c NULL, set to command completion code.
 * @return \c NULL on error, the pointer to the underlying buffer otherwise.
 *	The buffer gets silently overwritten on the next call of this function. 
 * @see	IPMI v2, 33.9
 */
sdr_repo_info_t *get_repo_info(uint8_t *cc);

/**
 * @brief Reserve SDR Repository Command.
 * @param cc		If not \c NULL, set to command completion code.
 * @return \c 0 on error, the obtained reservation ID otherwise.
 * @see	IPMI v2, 33.11 && 35.4
 */
uint16_t get_reservation(uint8_t *cc);

/**
 * @brief Get SDR Command. This implementation always fetches from offset \c 0
 *	and thus no repo reservation is needed. However, if some buggy
 *	implementations like SUN ILOMs return a \c SDR_CC_RESERVATION_CANCELED
 *	command completion code, this function automatically requests a new ID
 *	and reuses it as needed.
 *
 * @param record_id	The ID of the SDR to get. Use \c 0 to get the ID of the
 *	first avalable SDR. On success this gets replaced by the Id of the next
 *	available SDR. \c 0 means no valid response received, and \c 0xFFFF no more
 *	records available.
 * @param len		Number of SDR bytes to fetch. On return it contains the
 *	number of SDR bytes received, so on succcess it stays the same.
 * @param cc		If not \c NULL, set to command completion code.
 * @return \c NULL on error or if the returned command completion is \c != \c 0
 *	\c && \c != \c SDR_CC_BUFFER_TOO_SMALL (the later can be detected if the
 *	passed \c len parameter got changed). Otherwise a pointer to the start of
 *	the received SDR. The buffer gets silently overwritten on the next ipmi
 *	request.
 * @see	IPMI v2, 33.12 && 35.4 
 */
sdr_full_t *get_sdr(uint16_t *record_id, uint8_t *len, uint8_t *cc);

/**
 * @brief Get Sensor Thresholds Command.
 * @param snum	The unique number of the related sensor (SDR byte 8).
 * @param cc	If not \c NULL, it gets set to the completion code of the
 *	executed command. E.g. there might be an SDR for a fan sensor, but if the
 *	fan is not connected, the repo may return a \c SDR_CC_SENSOR_NOT_FOUND.
 *	In this case this function would silently return \c NULL, but the callee
 *	knows, its is intentional and not the result of an error. 
 * @return	\c NULL on error or if not available, a pointer to the buffered
 *	result otherwise.
 *	The buffer gets silently overwritten on the next ipmi request.
 * @see	IPMI v2, 35.9 
 */
sdr_thresholds_t *get_thresholds(uint8_t snum, uint8_t *cc);

/**
 * @brief Get Sensor Reading Command.
 * @param snum	The unique number of the related sensor (SDR byte 8).
 * qparam name	The name of the sensor to in diagnostic/debug messages.
 * @param cc		If not \c NULL, set to command completion code.
 * @return	\c NULL on error, a pointer to the buffered result otherwise.
 *	The buffer gets silently overwritten on the next ipmi request.
 * @see	IPMI v2, 35.14
 */
sdr_reading_t *get_reading(uint8_t snum, char *name, uint8_t *cc);

/**
 * @brief Get Sensor Reading Factors Command.
 * @param snum	The unique number of the related sensor (SDR byte 8).
 * @param reading	The current raw value of the sensor, which needs to be
 *	converted using the by this function returned factors.
 * @param cc	If not \c NULL, it gets set to the completion code of the
 *	executed command.
 * @return \c NULL on error or if not available, a pointer to the buffered
 *	result otherwise.
 *	The buffer gets silently overwritten on the next ipmi request.
 * @see	IPMI v2, 35.5
 */
sdr_factors_t *get_factors(uint8_t snum, uint8_t reading, uint8_t *cc);

/**
 * @brief DCMI Get Power Reading Command.
 * @param cc	If not \c NULL, set to command completion code. E.g. if it
 *	returns \c SDR_CC_INVALID_CMD you can be sure, that the BMC does not
 *	support this command and will never provide something useable.
 * @return \c NULL on error or if BMC does not support this command, a pointer
 *	to the buffered result otherwise.
 *	The buffer gets silently overwritten on the next ipmi request.
 * @see DCMI v1.5, table 6-16, Get Power Reading Command. (6.6.1)
 */
sdr_power_t *get_power(uint8_t *cc);

/**
 * @brief Release all resources associated with the given sensor in a recurive
 *	way.
 * @param sensor	Sensor to de-allocate. Ignored if \c NULL.
 * @note All via sensor->next connected sensors get released recursively as
 *	well. So set next to \c NULL if you want to release the given sensor, only.
 */
void free_sensor(sensor_t *sensor);

/**
 * @brief Scan the SDR repository for **FULL** threshold based SDRs providing
 *	non-discrete readings, arrange sensors found in a list and finally return
 *	the head of the list.
 * @param count	The number of sensors in the returned list.
 * @param ignore_disabled	Some bogus firmware like DEll's iDRAC crap report
 *	sensors as disabled in the related SDR capabilities, but actually they are
 *	neither disabled nor un-readable. If this parameter is set to \c true, this
 *	property gets ignored. Per default, sensors marked as disabled get dropped,
 *	i.e. do not appear in the returned sensor list.
 * @param drop_noread	If set, check immediately, whether the sensor returnes
 *	a reading. If the reading attempt returns the command completion code 
 *	\c SDR_CC_CMD_TMP_UNSUPPORTED (like Sun ILOMs do for
 *	not-yet populated/connected devices), the related sensor gets dropped, i.e.
 *	does not appear in the returned sensor list.
 */
sensor_t *scan_sdr_repo(uint32_t *count, bool ignore_disabled, bool drop_noread, uint8_t *cc);

/**
 * @brief	Check whether the repo has been changed since last call of this
 *	function. 
 * @return \c false if all sensors within the given list still are still
 *	assigned to the same SDR, not new records have been added or got deleted.
 *	Otherwise \c true, i.e. one should create a new sensor list and drop the
 *	old one e.g. to avoid using wrong thresholds and convertion factors.
 */
bool sdrs_changed(sensor_t *head);

/**
 * @brief Get the values of the given list of sensors, format them and related
 *	thresholds in '\c ipmitool \c sensor' format and store the result into the
 *	given string builder \c sb or print it out to stdout.
 * @param list	The list of sensors to query.
 * @param sb	The string builder to use to store the result. If \c NULL, the
 *	result gets pushed to \c stdout.
 * @param extended	If \c true, a SDR ID and sensor number column gets added
 *	as column 0 and 1, and a threshold state column added to the default
 *	output.
 */
void show_ipmitool_sensors(sensor_t *list, psb_t *sb, bool extended);

#ifdef __cplusplus
}
#endif

#endif  // IPMIMEX_IPMI_SDR_H
