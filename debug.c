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
#include <unistd.h>
#include <string.h>

#include "jtag.h"

static JTAG *jtag;

// Communicate with a simple JTAG debug register interface:
//
// 35  31                             0
// |   |                              |
// WAAADDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
//
// On DRUPDATE, if W=1, write data (D) to register address (A)
//              if W=0, read from register address (A), ignore data (D)
// On DRCAPTURE, set D to data from last read operation
//
// See an implementation for xilinx 7-series here:
// https://github.com/swetland/zynq-sandbox/blob/master/hdl/jtag_debug_port.sv

void jconnect(void) {
	if (jtag_mpsse_open(&jtag)) goto fail;
	if (jtag_enumerate(jtag) < 0) goto fail;
	if (jtag_select_by_family(jtag, "Xilinx 7")) goto fail;
	return;
fail:
	fprintf(stderr, "debug: cannot connect to fpga via jtag\n");
	exit(1);
}

u32 jrd(u32 addr) {
	u32 n = 0x23;
	u64 u = 0;
	jtag_ir_wr(jtag, 6, &n);
	n = addr & 7;
	jtag_dr_wr(jtag, 4, &n);
	jtag_dr_rd(jtag, 36, &u);
	jtag_commit(jtag);
	return (u32) u;
}

void jwr(u32 addr, u32 val) {
	u32 n = 0x23;
	u64 u = ((u64)val) | (((u64) (addr & 7)) << 32) | 0x800000000ULL;
	jtag_ir_wr(jtag, 6, &n);
	jtag_dr_wr(jtag, 36, &u);
	jtag_commit(jtag);
}

int main(int argc, char **argv) {
	if (argc == 3) {
		u32 addr = strtoul(argv[1], 0, 0);
		u32 val = strtoul(argv[2], 0, 0);
		jconnect();
		jwr(addr, val);
		return 0;
	} else if (argc == 2) {
		u32 addr = strtoul(argv[1], 0, 0);
		u32 val;
		jconnect();
		if (!strcmp(argv[1],"dump")) {
			int n, off;
			for (off = 0, n = 0; n < 4096; n++, off++) {
				unsigned x = jrd(1);
				if (x & 0x100) {
					off = -1;
					continue;
				}
				if ((off & 15) == 0) printf("\n%08x", off);
				printf(" %02x", x);
			}
			printf("\n");
			return 0;
		}
 		val = jrd(addr);
		printf("%08x\n", val);
		return 0;
	} else {
		fprintf(stderr,
			"usage:\n"
			"  write:  debug <addr> <val>\n"
			"   read:  debug <addr>\n");
		return -1;
	}
}
