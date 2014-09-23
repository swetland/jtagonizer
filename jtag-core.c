// Copyright 2014 Brian Swetland <swetland@frotz.net>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jtag-driver.h"


#define ZYNQID(code)	((0x1B<<21)|(0x9<<17)|((code)<<12)|(0x49<<1)|1)
#define ZYNQMASK	0x0FFFFFFF

static JTAG_INFO LIBRARY[] = {
	{ 0x4ba00477, 0xFFFFFFFF, 4, "Cortex A9" },
	{ ZYNQID(0x02), ZYNQMASK, 6, "xc7x010" },
	{ ZYNQID(0x1b), ZYNQMASK, 6, "xc7x015" },
	{ ZYNQID(0x07), ZYNQMASK, 6, "xc7x020" },
	{ ZYNQID(0x0c), ZYNQMASK, 6, "xc7x030" },
	{ ZYNQID(0x11), ZYNQMASK, 6, "xc7x045" },
	{ 0x13631093, 0xFFFFFFFF, 6, "xc7a100t" },
};

JTAG_INFO *jtag_lookup_device(unsigned idcode) {
	int n;
	for (n = 0; n < (sizeof(LIBRARY)/sizeof(LIBRARY[0])); n++) {
		if ((idcode & LIBRARY[n].idmask) == LIBRARY[n].idcode) {
			return LIBRARY + n;
		}
	}
	return NULL;
}

#define TRACE_STATEMACHINE 0

// configuration and state or IR or DR
typedef struct JREG {
	u8 *prebits;
	u8 *postbits;
	u32 precount;
	u32 postcount;
	u32 scanstate;
	u32 idlestate;
} JREG;

#define DEVMAX 32

// configuration and state of JTAG
struct JTAG {
	JDRV *drv;
	JDVT *vt;

	JREG ir;
	JREG dr;

	u32 state;

	int devcount;
	JTAG_INFO devinfo[DEVMAX];
};

#define _setspeed(khz) \
	jtag->vt->setspeed(jtag->drv, khz)
#define _commit() \
	jtag->vt->commit(jtag->drv)
#define _scan_tms(obit, count, tbits, ioffset, ibits) \
	jtag->vt->scan_tms(jtag->drv, obit, count, tbits, ioffset, ibits)
#define _scan_io(count, obits, ibits) \
	jtag->vt->scan_io(jtag->drv, count, obits, ibits)
#define _close() \
	jtag->vt->close(jtag->drv)

static u8 ONES[1024];

void jtag_clear_state(JTAG *jtag) {
	jtag->ir.idlestate = JTAG_IDLE;
	jtag->ir.scanstate = JTAG_IRSHIFT;
	jtag->dr.idlestate = JTAG_IDLE;
	jtag->dr.scanstate = JTAG_DRSHIFT;
	jtag->ir.prebits = 0;
	jtag->ir.precount = 0;
	jtag->ir.postbits = 0;
	jtag->ir.postcount = 0;
	jtag->dr.prebits = 0;
	jtag->dr.precount = 0;
	jtag->dr.postbits = 0;
	jtag->dr.postcount = 0;
}

int jtag_init(JTAG **_jtag, JDRV *drv, JDVT *vt) {
	JTAG *jtag;
	if ((jtag = malloc(sizeof(JTAG))) == 0) {
		return -1;
	}
	memset(jtag, 0, sizeof(JTAG));
	jtag->drv = drv;
	jtag->vt = vt;
	jtag_clear_state(jtag);
	*_jtag = jtag;
	memset(ONES, 0xFF, sizeof(ONES));
	return 0;
}

void jtag_close(JTAG *jtag) {
	_close();
	free(jtag);
}

int jtag_setspeed(JTAG *jtag, int khz) {
	return _setspeed(khz);
}

void jtag_set_ir_idle(JTAG *jtag, unsigned state) {
	jtag->ir.idlestate = state;
}

void jtag_set_dr_idle(JTAG *jtag, unsigned state) {
	jtag->dr.idlestate = state;
}

void jtag_set_ir_prefix(JTAG *jtag, unsigned count, const void *bits) {
	jtag->ir.prebits = count ? (u8*) bits : 0;
	jtag->ir.precount = count;
}
void jtag_set_ir_postfix(JTAG *jtag, unsigned count, const void *bits) {
	jtag->ir.postbits = count ? (u8*) bits : 0;
	jtag->ir.postcount = count;
}
void jtag_set_dr_prefix(JTAG *jtag, unsigned count, const void *bits) {
	jtag->dr.prebits = count ? (u8*) bits : 0;
	jtag->dr.precount = count;
}
void jtag_set_dr_postfix(JTAG *jtag, unsigned count, const void *bits) {
	jtag->dr.postbits = count ? (u8*) bits : 0;
	jtag->dr.postcount = count;
}

static u32 lastbit(const u8 *bits, u32 count) {
	count -= 1;
	return (bits[count >> 3] >> (count & 7)) & 1;
}

static const char *JSTATE[16] = {
	"RESET", "IDLE", "DRSELECT", "DRCAPTURE",
	"DRSHIFT", "DREXIT1", "DRPAUSE", "DREXIT2",
	"DRUPDATE", "IRSELECT", "IRCAPTURE", "IRSHIFT",
	"IREXIT1", "IRPAUSE", "IREXIT1", "IRUPDATE"
};

#define JPATH(x,c) { static u8 out = x; *bits = &out; return c; }

static u32 jtag_plot(u32 from, u32 to, u8 **bits) {
#if TRACE_STATEMACHINE
	fprintf(stderr,"jtag_plot: move from %s to %s\n",
			JSTATE[from], JSTATE[to]);
#endif
	switch (from) {
	case JTAG_RESET:
		if (to == JTAG_IDLE) JPATH(0x00, 1); // 0
		if (to == JTAG_IRSHIFT) JPATH(0x06, 5); // 01100
		if (to == JTAG_DRSHIFT) JPATH(0x02, 4); // 0100
		break;
	case JTAG_IDLE:
		if (to == JTAG_IRSHIFT) JPATH(0x03, 4); // 1100
		if (to == JTAG_DRSHIFT) JPATH(0x01, 3); // 100
		break;
	case JTAG_IRSHIFT:
		if (to == JTAG_IDLE) JPATH(0x03, 3); // 110
		if (to == JTAG_IRPAUSE) JPATH(0x01, 2); // 10
		break;
	case JTAG_DRSHIFT:
		if (to == JTAG_IDLE) JPATH(0x03, 3); // 110
		if (to == JTAG_DRPAUSE) JPATH(0x01, 2); // 10
		break;
	case JTAG_IRPAUSE:
		if (to == JTAG_IDLE) JPATH(0x03, 3); // 110
		if (to == JTAG_IRSHIFT) JPATH(0x01, 2); // 10
		if (to == JTAG_DRSHIFT) JPATH(0x07, 5); // 11100
		break;
	case JTAG_DRPAUSE:
		if (to == JTAG_IDLE) JPATH(0x03, 3); // 110
		if (to == JTAG_DRSHIFT) JPATH(0x01, 2); // 10
		if (to == JTAG_IRSHIFT) JPATH(0x0F, 6); // 111100
		break;
	}
	if (to == JTAG_RESET) JPATH(0x3F, 6); // 111111

	fprintf(stderr,"jtag_plot: cannot move from %s to %s\n",
			JSTATE[from], JSTATE[to]);
	return 0;
};

void jtag_goto(JTAG *jtag, unsigned state) {
	u32 mcount;
	u8 *mbits;
	mcount = jtag_plot(jtag->state, state, &mbits);
	if (mcount != 0) {
		_scan_tms(0, mcount, mbits, 0, 0);
		jtag->state = state;
	}
}

static void jtag_xr_wr(JTAG *jtag, JREG *xr, u32 count, u8 *wbits) {
	u32 mcount;
	u8 *mbits;
	jtag_goto(jtag, xr->scanstate);
	mcount = jtag_plot(xr->scanstate, xr->idlestate, &mbits);
	if (xr->prebits) {
		_scan_io(xr->precount, xr->prebits, 0);
	}
	if (xr->postbits) {
		_scan_io(count, wbits, 0);
		_scan_io(xr->postcount - 1, xr->postbits, 0);
		_scan_tms(lastbit(xr->postbits, xr->postcount), mcount, mbits, 0, 0);
	} else {
		_scan_io(count - 1, wbits, 0);
		_scan_tms(lastbit(wbits, count), mcount, mbits, 0, 0);
	}
	jtag->state = xr->idlestate;
}

static void jtag_xr_rd(JTAG *jtag, JREG *xr, u32 count, u8 *rbits) {
	u32 mcount;
	u8 *mbits;
	jtag_goto(jtag, xr->scanstate);
	mcount = jtag_plot(xr->scanstate, xr->idlestate, &mbits);
	if (xr->prebits) {
		_scan_io(xr->precount, xr->prebits, 0);
	}
	if (xr->postbits) {
		_scan_io(count, 0, rbits);
		_scan_io(xr->postcount - 1, xr->postbits, 0);
		_scan_tms(lastbit(xr->postbits, xr->postcount), mcount, mbits, 0, 0);
	} else {
		_scan_io(count - 1, 0, rbits);
		_scan_tms(0, mcount, mbits, count - 1, rbits);
	}
	jtag->state = xr->idlestate;
}

static void jtag_xr_io(JTAG *jtag, JREG *xr, u32 count, u8 *wbits, u8 *rbits) {
	u32 mcount;
	u8 *mbits;
	jtag_goto(jtag, xr->scanstate);
	mcount = jtag_plot(xr->scanstate, xr->idlestate, &mbits);
	if (xr->prebits) {
		_scan_io(xr->precount, xr->prebits, 0);
	}
	if (xr->postbits) {
		_scan_io(count, (void*) wbits, rbits);
		_scan_io(xr->postcount - 1, xr->postbits, 0);
		_scan_tms(lastbit(xr->postbits, xr->postcount), mcount, mbits, 0, 0);
	} else {
		_scan_io(count - 1, (void*) wbits, rbits);
		_scan_tms(lastbit(wbits, count), mcount, mbits, count - 1, rbits);
	}
	jtag->state = xr->idlestate;
}

void jtag_ir_wr(JTAG *jtag, unsigned count, const void *wbits) {
	jtag_xr_wr(jtag, &jtag->ir, count, (void*) wbits);
}
void jtag_ir_rd(JTAG *jtag, unsigned count, void *rbits) {
	jtag_xr_rd(jtag, &jtag->ir, count, rbits);
}
void jtag_ir_io(JTAG *jtag, unsigned count, const void *wbits, void *rbits) {
	jtag_xr_io(jtag, &jtag->ir, count, (void*) wbits, rbits);
}

void jtag_dr_wr(JTAG *jtag, unsigned count, const void *wbits) {
	jtag_xr_wr(jtag, &jtag->dr, count, (void*) wbits);
}
void jtag_dr_rd(JTAG *jtag, unsigned count, void *rbits) {
	jtag_xr_rd(jtag, &jtag->dr, count, rbits);
}
void jtag_dr_io(JTAG *jtag, unsigned count, const void *wbits, void *rbits) {
	jtag_xr_io(jtag, &jtag->dr, count, (void*) wbits, rbits);
}

int jtag_commit(JTAG *jtag) {
	return _commit();
}

int jtag_enumerate(JTAG *jtag) {
	JTAG_INFO *info;
	u32 data[DEVMAX];
	int n;

	jtag_clear_state(jtag);
	jtag->devcount = 0;

	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_dr_io(jtag, DEVMAX * 4 * 8, ONES, data);
	if (jtag_commit(jtag)) {
		return -1;
	}
	for (n = 0; n < DEVMAX; n++) {

		if (data[n] == 0xffffffff) {
			if (n == 0) {
				fprintf(stderr, "no devices found\n");
				return -1;
			}
			jtag->devcount = n;
			return n;
		}
		if (!(data[n] & 1)) {
			fprintf(stderr, "device %d has no idcode\n", n);
			return -1;
		}
		if ((info = jtag_lookup_device(data[n])) == NULL) {
			fprintf(stderr, "device %d (id %08x) unknown\n",
				n, (unsigned) data[n]);
			return -1;
		}
		memcpy(jtag->devinfo + n, info, sizeof(JTAG_INFO));
		fprintf(stderr, "device %02d idcode %08x '%s'\n",
			n, info->idcode, info->name);
	}
	fprintf(stderr, "too many devices\n");
	return -1;
}

JTAG_INFO *jtag_get_nth_device(JTAG *jtag, int n) {
	if ((n >= jtag->devcount) || (n < 0)) {
		return NULL;
	}
	return jtag->devinfo + n;
}

int jtag_select_device_nth(JTAG *jtag, int num) {
	u32 irpre = 0;
	u32 irpost = 0;
	u32 drpre = 0;
	u32 drpost = 0;
	int n;
	if ((num >= jtag->devcount) || (num < 0)) {
		return -1;
	}
	for (n = 0; n < jtag->devcount; n++) {
		if (n < num) {
			irpre += jtag->devinfo[n].irsize;
			drpre += 1;
		} else if (n > num) {
			irpost += jtag->devinfo[n].irsize;
			drpost += 1;
		}
	}
	jtag_set_ir_prefix(jtag, irpre, ONES);
	jtag_set_ir_postfix(jtag, irpost, ONES);
	jtag_set_dr_prefix(jtag, drpre, ONES);
	jtag_set_dr_postfix(jtag, drpost, ONES);
	return 0;
}

int jtag_select_device(JTAG *jtag, unsigned idcode) {
	int n;
	for (n = 0; n < jtag->devcount; n++) {
		if (jtag->devinfo[n].idcode == idcode) {
			return jtag_select_device_nth(jtag, n);
		}
	}
	return -1;
}

