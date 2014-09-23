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
#include <string.h>

#include "jtag.h"
#include "dap.h"

typedef struct {
	JTAG *jtag;
	u32 cache_ir;
	u32 cache_apnum;
	u32 cache_apaddr;
} DAP;

int dap_ir_wr(DAP *dap, u32 ir) {
	if (dap->cache_ir != ir) {
		jtag_ir_wr(dap->jtag, 4, &ir);
		if (jtag_commit(dap->jtag)) {	
			dap->cache_ir = 0xFFFFFFFF;
			return -1;
		}
	}
	return 0;
}

int dap_dr_io(DAP *dap, u32 bitcount, u64 wdata, u64 *rdata) {
	if (rdata) {
		*rdata = 0;
		jtag_dr_io(dap->jtag, bitcount, &wdata, rdata);
	} else {
		jtag_dr_wr(dap->jtag, bitcount, &wdata);
	}
	return jtag_commit(dap->jtag);
}

int dap_dp_rd(DAP *dap, u32 addr, u32 *val) {
	int retry = 30;
	u64 u;
	dap_ir_wr(dap, DAP_IR_DPACC);
	dap_dr_io(dap, 35, XPACC_RD(addr), NULL);
	while (retry-- > 0) {
		dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
		switch (XPACC_STATUS(u)) {
		case XPACC_OK:
			*val = u >> 3;
			return 0;
		case XPACC_WAIT:
			fprintf(stderr,"!");
			continue;
		default:
			return -1;
		}
	}
	return -1;
}

int dap_dp_wr(DAP *dap, u32 addr, u32 val) {
	int retry = 100;
	u64 u;
	u = XPACC_WR(addr);
	u |= (((u64) val) << 3);
	dap_ir_wr(dap, DAP_IR_DPACC);
	dap_dr_io(dap, 35, u, NULL);
	while (retry-- > 0) {
		dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
		switch (XPACC_STATUS(u)) {
		case XPACC_OK:
			return 0;
		case XPACC_WAIT:
			fprintf(stderr,"!");
			continue;
		default:
			return -1;
		}
	}
	return -1;
}

/* TODO: cache state, check errors */
int dap_ap_rd(DAP *dap, u32 apnum, u32 addr, u32 *val) {
	u64 u;
	if ((dap->cache_apnum != apnum) || (dap->cache_apaddr != (addr & 0xFFFFFFF0))) {
		if (dap_dp_wr(dap, DPACC_SELECT,
			DPSEL_APSEL(apnum) | DPSEL_APBANKSEL(addr))) {
			dap->cache_apnum = 0xFFFFFFFF;
			dap->cache_apaddr = 0xFFFFFFFF;
			return -1;
		}
		dap->cache_apnum = apnum;
		dap->cache_apaddr = addr;
	}
	dap_ir_wr(dap, DAP_IR_APACC);
	dap_dr_io(dap, 35, XPACC_RD(addr), NULL);
	dap_ir_wr(dap, DAP_IR_DPACC);
	dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
	*val = (u >> 3);
	return 0;
}

int dap_ap_wr(DAP *dap, u32 apnum, u32 addr, u32 val) {
	u64 u;
	if ((dap->cache_apnum != apnum) || (dap->cache_apaddr != (addr & 0xFFFFFFF0))) {
		if (dap_dp_wr(dap, DPACC_SELECT,
			DPSEL_APSEL(apnum) | DPSEL_APBANKSEL(addr))) {
			dap->cache_apnum = 0xFFFFFFFF;
			dap->cache_apaddr = 0xFFFFFFFF;
			return -1;
		}
		dap->cache_apnum = apnum;
		dap->cache_apaddr = addr;
	}
	dap_ir_wr(dap, DAP_IR_APACC);
	u = XPACC_WR(addr);
	u |= (((u64) val) << 3);
	dap_dr_io(dap, 35, u, NULL);
	dap_ir_wr(dap, DAP_IR_DPACC);
	dap_dr_io(dap, 35, XPACC_RD(DPACC_RDBUFF), &u);
	return 0;
}

int dap_ap_wr_x(DAP *dap, u32 apnum, u32 addr, u32 val) {
	u64 u;
	if ((dap->cache_apnum != apnum) || (dap->cache_apaddr != (addr & 0xFFFFFFF0))) {
		if (dap_dp_wr(dap, DPACC_SELECT,
			DPSEL_APSEL(apnum) | DPSEL_APBANKSEL(addr))) {
			dap->cache_apnum = 0xFFFFFFFF;
			dap->cache_apaddr = 0xFFFFFFFF;
			return -1;
		}
		dap->cache_apnum = apnum;
		dap->cache_apaddr = addr;
	}
	dap_ir_wr(dap, DAP_IR_APACC);
	u = XPACC_WR(addr);
	u |= (((u64) val) << 3);
	dap_dr_io(dap, 35, u, NULL);
	return 0;
}
void dap_flush_cache(DAP *dap) {
	dap->cache_ir = 0xFFFFFFFF;
	dap->cache_apnum = 0xFFFFFFFF;
	dap->cache_apaddr = 0xFFFFFFFF;
}

DAP *dap_init(JTAG *jtag) {
	DAP *dap = malloc(sizeof(DAP));
	memset(dap, 0, sizeof(DAP));
	dap->jtag = jtag;
	dap_flush_cache(dap);
	return dap;
}

int dap_attach(DAP *dap) {
	unsigned n;
	u32 x;
	// attempt to power up and clear errors
	for (n = 0; n < 100; n++) {
		if (dap_dp_wr(dap, DPACC_CSW, 
			DPCSW_CSYSPWRUPREQ | DPCSW_CDBGPWRUPREQ |
			DPCSW_STICKYERR | DPCSW_STICKYCMP | DPCSW_STICKYORUN))
			continue;
		if (dap_dp_rd(dap, DPACC_CSW, &x))
			continue;
		if (x & (DPCSW_STICKYERR | DPCSW_STICKYCMP | DPCSW_STICKYORUN))
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

int dap_mem_wr32(DAP *dap, u32 addr, u32 val) {
	if (addr & 3)
		return -1;
	if (dap_ap_wr(dap, 0, APACC_TAR, addr))
		return -1;
	if (dap_ap_wr(dap, 0, APACC_DRW, val))
		return -1;
	return 0;
}

int dap_mem_rd32(DAP *dap, u32 addr, u32 *val) {
	if (addr & 3)
		return -1;
	if (dap_ap_wr(dap, 0, APACC_TAR, addr))
		return -1;
	if (dap_ap_rd(dap, 0, APACC_DRW, val))
		return -1;
	return 0;
}

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;
	unsigned n;
	u64 u;
	u32 x;

	if (jtag_mpsse_open(&jtag)) return -1;
	if (jtag_enumerate(jtag) < 0) return -1;
	if (jtag_select_device(jtag, 0x4ba00477)) return -1;

	dap = dap_init(jtag);
	if (dap_attach(dap))
		return -1;

	for (n = 0; n < 256; n++) {
		if (dap_ap_rd(dap, n, APACC_IDR, &x))
			break;
		if (x == 0)
			break;
		printf("AP%d ID=%08x\n", n, x);
		if (dap_ap_rd(dap, n, APACC_CSW, &x) == 0)
			printf("AP%d CSW=%08x\n", n, x);
	}

#if 1 
	for (n = 0; n < 8; n++) {
		dap_mem_rd32(dap, n*4, &x);
		printf("%08x: %08x\n", n*4, x);
	}
#endif
	
#if 0
	for (n = 0; n < 4096; n++) {
		dap_mem_wr32(dap, n*4, 0xeeeeeeee);
	}
	return 0;
#endif

#if 0 
	dap_ap_wr(dap, 0, APACC_CSW, APCSW_DBGSWEN | APCSW_INCR_SINGLE | APCSW_SIZE32);
	dap_ap_wr(dap, 0, APACC_TAR, n);
	for (n = 0; n < 4096; n++) {
		dap_ap_wr_x(dap, 0, APACC_DRW, 0xaaaaaaaa);
	}
	return 0;
#endif
	return 0;
}
