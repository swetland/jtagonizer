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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jtag.h"
#include "dap.h"

#define CSW_ERRORS (DPCSW_STICKYERR | DPCSW_STICKYCMP | DPCSW_STICKYORUN)
#define CSW_ENABLES (DPCSW_CSYSPWRUPREQ | DPCSW_CDBGPWRUPREQ | DPCSW_ORUNDETECT)

#include <time.h>
static u64 NOW(void) {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) return 0;
	return (((u64) ts.tv_sec) * ((u64)1000000000)) + ((u64) ts.tv_nsec);
}

typedef struct {
	JTAG *jtag;
	u32 cached_ir;
} DAP;

static void q_dap_ir_wr(DAP *dap, u32 ir) {
	if (dap->cached_ir != ir) {
		dap->cached_ir = ir;
		jtag_ir_wr(dap->jtag, 4, &ir);
	}
}

// force ir write even if redundant
// used for a timing hack
static void _q_dap_ir_wr(DAP *dap, u32 ir) {
	dap->cached_ir = ir;
	jtag_ir_wr(dap->jtag, 4, &ir);
}

static void q_dap_dr_io(DAP *dap, u32 bitcount, u64 wdata, u64 *rdata) {
	if (rdata) {
		*rdata = 0;
		jtag_dr_io(dap->jtag, bitcount, &wdata, rdata);
	} else {
		jtag_dr_wr(dap->jtag, bitcount, &wdata);
	}
}

static void q_dap_abort(DAP *dap) {
	u32 x;
	u64 u;
	x = DAP_IR_ABORT;
	u = 8;
	jtag_ir_wr(dap->jtag, 4, &x);
	jtag_dr_wr(dap->jtag, 35, &u);
	dap->cached_ir = 0xFFFFFFFF;
}

// queue a DPCSW status query, commit jtag txn
static int dap_commit(DAP *dap) {
	u64 a, b;
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dr_io(dap, 35, XPACC_RD(DPACC_CSW), &a);
	q_dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &b);
	dap->cached_ir = 0xFFFFFFFF;
	if (jtag_commit(dap->jtag)) {
		return -1;
	}
	if (XPACC_STATUS(a) != XPACC_OK) {
		fprintf(stderr, "dap: invalid txn status\n");
		return -1;
	}
	if (XPACC_STATUS(b) != XPACC_OK) {
		fprintf(stderr, "dap: cannot read status\n");
		return -1;
	}
	b >>= 3;
	if (b & DPCSW_STICKYORUN) {
		fprintf(stderr, "dap: overrun\n");
		return -1;
	}
	if (b & DPCSW_STICKYERR) {
		fprintf(stderr, "dap: error\n");
		return -1;
	}
	return 0;
}

int dap_dp_rd(DAP *dap, u32 addr, u32 *val) {
	u64 u;
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dr_io(dap, 35, XPACC_RD(addr), NULL);
	q_dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
	if (dap_commit(dap)) {
		return -1;
	}
	*val = u >> 3;
	return 0;
}

static void q_dap_dp_wr(DAP *dap, u32 addr, u32 val) {
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dr_io(dap, 35, XPACC_WR(addr, val), NULL);
	//q_dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &s->u);
}

int dap_dp_wr(DAP *dap, u32 addr, u32 val) {
	q_dap_dp_wr(dap, addr, val);
	return dap_commit(dap);
}

int dap_ap_rd(DAP *dap, u32 apnum, u32 addr, u32 *val) {
	u64 u;
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dp_wr(dap, DPACC_SELECT, DPSEL_APSEL(apnum) | DPSEL_APBANKSEL(addr));
	q_dap_ir_wr(dap, DAP_IR_APACC);
	q_dap_dr_io(dap, 35, XPACC_RD(addr), NULL);
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
	// TODO: redundant ir wr
	if (dap_commit(dap)) {
		return -1;
	}
	*val = (u >> 3);
	return 0;
}

void q_dap_ap_wr(DAP *dap, u32 apnum, u32 addr, u32 val) {
	q_dap_ir_wr(dap, DAP_IR_DPACC);
	q_dap_dp_wr(dap, DPACC_SELECT, DPSEL_APSEL(apnum) | DPSEL_APBANKSEL(addr));
	q_dap_ir_wr(dap, DAP_IR_APACC);
	q_dap_dr_io(dap, 35, XPACC_WR(addr, val), NULL);
}

int dap_mem_wr32(DAP *dap, u32 n, u32 addr, u32 val) {
	if (addr & 3)
		return -1;
	q_dap_ap_wr(dap, n, APACC_CSW,
		APCSW_DBGSWEN | APCSW_INCR_NONE | APCSW_SIZE32);
	q_dap_ap_wr(dap, n, APACC_TAR, addr);
	q_dap_ap_wr(dap, n, APACC_DRW, val);
	return dap_commit(dap);
}

int dap_mem_rd32(DAP *dap, u32 n, u32 addr, u32 *val) {
	if (addr & 3)
		return -1;
	q_dap_ap_wr(dap, n, APACC_TAR, addr);
	if (dap_ap_rd(dap, n, APACC_DRW, val))
		return -1;
	return 0;
}

int dap_mem_read(DAP *dap, u32 apnum, u32 addr, void *data, u32 len) {
	u64 scratch[1024];
	u32 *x = data;
	u32 n;

	if ((addr & 3) || (((u64) data) & 3)) {
		// base and length must be aligned
		return -1;
	}

	while (len > 0) {
		// max transfer is 1K
		// transfer may not cross 1K boundary
		u32 xfer = 1024 - (addr & 0x3FF);
		if (xfer > len) {
			xfer = len;
		}
		q_dap_ap_wr(dap, apnum, APACC_CSW,
			APCSW_DBGSWEN | APCSW_INCR_SINGLE | APCSW_SIZE32);
		q_dap_dr_io(dap, 35, XPACC_WR(APACC_TAR, addr), NULL);
		// read txn will be returned on the next txn
		q_dap_dr_io(dap, 35, XPACC_RD(APACC_DRW), NULL);
		_q_dap_ir_wr(dap, DAP_IR_APACC); // HACK, timing
		for (n = 0; n < (xfer-4); n += 4) {
			q_dap_dr_io(dap, 35, XPACC_RD(APACC_DRW), &scratch[n/4]);
			_q_dap_ir_wr(dap, DAP_IR_APACC); // HACK, timing
		}
		// dummy read of TAR to pick up last read value
		q_dap_dr_io(dap, 35, XPACC_RD(APACC_TAR), &scratch[n/4]);
		if (dap_commit(dap)) {
			return -1;
		}
		for (n = 0; n < xfer; n += 4) {
			switch(XPACC_STATUS(scratch[n/4])) {
			case XPACC_WAIT: fprintf(stderr,"w"); break;
			case XPACC_OK: fprintf(stderr,"o"); break;
			default: fprintf(stderr,"?"); break;
			}
			*x++ = scratch[n/4] >> 3;
		}
		fprintf(stderr,"\n");
		len -= xfer;
		addr += xfer;
	}
	return 0;
}

int dap_mem_write(DAP *dap, u32 apnum, u32 addr, void *data, u32 len) {
	u32 *x = data;
	u32 n;

	if ((addr & 3) || (((u64) data) & 3)) {
		// base and length must be aligned
		return -1;
	}

	while (len > 0) {
		// max transfer is 1K
		// transfer may not cross 1K boundary
		u32 xfer = 1024 - (addr & 0x3FF);
		if (xfer > len) {
			xfer = len;
		}
		q_dap_ap_wr(dap, apnum, APACC_CSW, APCSW_DBGSWEN | APCSW_INCR_SINGLE | APCSW_SIZE32);
		q_dap_dr_io(dap, 35, XPACC_WR(APACC_TAR, addr), NULL);
		for (n = 0; n < xfer; n += 4) {
			_q_dap_ir_wr(dap, DAP_IR_APACC); // HACK, timing
			q_dap_dr_io(dap, 35, XPACC_WR(APACC_DRW, *x++), NULL);
		}
		if (dap_commit(dap)) {
			return -1;
		} 
		len -= xfer;
		addr += xfer;
	}
	return 0;
}

DAP *dap_init(JTAG *jtag) {
	DAP *dap = malloc(sizeof(DAP));
	memset(dap, 0, sizeof(DAP));
	dap->jtag = jtag;
	dap->cached_ir = 0xFFFFFFFF;
	return dap;
}

int dap_attach(DAP *dap) {
	unsigned n;
	u32 x;

	// make sure we abort any ongoing transactions first
	q_dap_abort(dap);

	// attempt to power up and clear errors
	for (n = 0; n < 10; n++) {
		if (dap_dp_wr(dap, DPACC_CSW, CSW_ERRORS | CSW_ENABLES))
			continue;
		if (dap_dp_rd(dap, DPACC_CSW, &x))
			continue;
		if (x & CSW_ERRORS)
			continue;
		if (!(x & DPCSW_CSYSPWRUPACK))
			continue;
		if (!(x & DPCSW_CDBGPWRUPACK))
			continue;
		return 0;
	}
	fprintf(stderr,"dap: attach failed\n");
	return -1;
}

static int read4xid(DAP *dap, u32 n, u32 addr, u32 *val) {
	u32 a,b,c,d;
	if (dap_mem_rd32(dap, n, addr + 0x00, &a)) return -1;
	if (dap_mem_rd32(dap, n, addr + 0x04, &b)) return -1;
	if (dap_mem_rd32(dap, n, addr + 0x08, &c)) return -1;
	if (dap_mem_rd32(dap, n, addr + 0x0C, &d)) return -1;
	*val = (a & 0xFF) | ((b & 0xFF) << 8) | ((c & 0xFF) << 16) | ((d & 0xFF) << 24);
	return 0;
}

static int readinfo(DAP *dap, u32 n, u32 base, u32 *cid, u32 *pid0, u32 *pid1) {
	if (read4xid(dap, n, base + 0xFF0, cid)) return -1;
	if (read4xid(dap, n, base + 0xFE0, pid0)) return -1;
	if (read4xid(dap, n, base + 0xFD0, pid1)) return -1;
	return 0;
}

static void dumptable(DAP *dap, u32 n, u32 base) {
	u32 cid, pid0, pid1, memtype;
	u32 x, addr;
	int i;

	printf("TABLE   @%08x ", base);
	if (readinfo(dap, n, base, &cid, &pid0, &pid1)) {
		printf("<error reading cid & pid>\n");
		return;
	}
	if (dap_mem_rd32(dap, n, base + 0xFCC, &memtype)) {
		printf("<error reading memtype>\n");
		return;
	}
	printf("CID %08x  PID %08x %08x  %dKB%s\n", cid, pid1, pid0,
		4 * (1 + ((pid1 & 0xF0) >> 4)),
		(memtype & 1) ? "  SYSMEM": "");
	for (i = 0; i < 128; i++) {
		if (dap_mem_rd32(dap, n, base + i * 4, &x)) break;
		if (x == 0) break;
		if ((x & 3) != 3) continue;
		addr = base + (x & 0xFFFFF000);
		if (readinfo(dap, n, addr, &cid, &pid0, &pid1)) {
			printf("    <error reading cid & pid>\n");
			continue;
		}
		printf("    %02d: @%08x CID %08x  PID %08x %08x  %dKB\n",
			i, addr, cid, pid1, pid0,
			4 * (1 + ((pid1 & 0xF0) >> 4)));
		if (((cid >> 12) & 0xF) == 1) {
			dumptable(dap, n, addr);
		}
	}
}

int dap_probe(DAP *dap) {
	unsigned n;
	u32 x, y;

	for (n = 0; n < 256; n++) {
		if (dap_ap_rd(dap, n, APACC_IDR, &x))
			break;
		if (x == 0)
			break;
		y = 0;
		dap_ap_rd(dap, n, APACC_BASE, &y);
		printf("AP%d ID=%08x BASE=%08x\n", n, x, y);
		if (y && (y != 0xFFFFFFFF)) {
			dumptable(dap, n, y);
		}
		if (dap_ap_rd(dap, n, APACC_CSW, &x) == 0)
			printf("AP%d CSW=%08x\n", n, x);
	}
	return 0;
}

u32 DATA[1024*1024];

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;
	unsigned n;
	u32 x;

	if (jtag_mpsse_open(&jtag)) return -1;
	if (jtag_enumerate(jtag) < 0) return -1;
	if (jtag_select_device(jtag, 0x4ba00477)) return -1;

	dap = dap_init(jtag);
	if (dap_attach(dap))
		return -1;

	dap_dp_rd(dap, DPACC_CSW, &x);
	printf("DPCSW=%08x\n", x);

	dap_probe(dap);

#if 1 
	for (n = 0; n < 8; n++) {
		x = 0xefefefef;
		dap_mem_rd32(dap, 0, 0x00000000 + n*4, &x);
		printf("%08x: %08x\n", n*4, x);
	}
#endif

#if 0
	for (n = 0; n < 1024*1024; n++) DATA[n] = n;
	for (n = 0; n < 10; n++)
		dap_mem_write(dap, 0, 0, DATA, 192*1024);
#endif

#if 0 
	if (dap_mem_read(dap, 0, 0, DATA, 4096) == 0) {
		for (n = 0; n < 16; n++) printf("%08x ",DATA[n]);
		printf("\n");
	}
	//dap_mem_wr32(dap, 0, 0x10, 0x805038a1);
	dap_dp_rd(dap, DPACC_CSW, &x);
	printf("DPCSW=%08x\n", x);
#endif
	return 0;
}
