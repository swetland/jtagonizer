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
#include "v7debug.h"

typedef struct V7DEBUG V7DEBUG;

struct V7DEBUG {
	DAP *dap;
	u32 apnum;
	u32 base;
};

static inline int dwr(V7DEBUG *debug, u32 off, u32 val) {
	return dap_mem_wr32(debug->dap, debug->apnum, debug->base + off, val);
}

static inline int drd(V7DEBUG *debug, u32 off, u32 *val) {
	return dap_mem_rd32(debug->dap, debug->apnum, debug->base + off, val);
}

V7DEBUG *debug_init(DAP *dap, u32 apnum, u32 base) {
	V7DEBUG *debug = malloc(sizeof(V7DEBUG));
	if (debug == NULL) {
		return NULL;
	}
	memset(debug, 0, sizeof(V7DEBUG));
	debug->dap = dap;
	debug->apnum = apnum;
	debug->base = base;
	return debug;
}


#define P(n) do { u32 x; if (!drd(debug, n, &x)) printf("%s: %08x\n", #n, x); } while(0)

int dccrd(V7DEBUG *debug, u32 *val) {
	u32 x;
	int n;
	for (n = 0; n < 10; n++) {
		if (drd(debug, DBGDSCR, &x)) return -1;
		if (x & DSCR_TXFULL) {
			return drd(debug, DBGDTRTX, val);
		}
	}
	fprintf(stderr, "v7debug: dcc read timed out\n");
	return -1;
}

int dexec(V7DEBUG *debug, u32 instr) {
	u32 x;
	int n;
	dwr(debug, DBGITR, instr);
	for (n = 0; n < 10; n++) {
		if (drd(debug, DBGDSCR, &x)) return -1;
		if (x & DSCR_INSTRCOMPL) return 0;
	}
	fprintf(stderr, "v7debug: instruction timed out\n");
	return -1;
}

#define ARM_DSB			0xE57FF040
#define ARM_MOV_DCC_Rx(x)	(0xEE000E15 | ((x) << 12)) // Rx -> DCC
#define ARM_MOV_Rx_DCC(x)	(0xEE100E15 | ((x) << 12)) // DCC -> Rx
#define ARM_MOV_R0_PC		0xE1A0000F
#define ARM_MOV_CPSR_R0		0xE12FF000 // R0 -> CPSR
#define ARM_MOV_R0_CPSR		0xE10F0000 // CPSR -> R0

int debug_attach(V7DEBUG *debug) {
	u32 x;
	u32 r[16];
	u32 cpsr;
	int n;

	if (dap_attach(debug->dap)) return -1;
	P(DBGDIDR);
	P(DBGDEVID);
	P(DBGDEVTYPE);
	P(DBGLSR);
	P(DBGDSCCR);
	P(DBGDSCR);

	dwr(debug, DBGDRCR, DRCR_HALT_REQ);
	for (n = 0; n < 100; n++) {
		if (drd(debug, DBGDSCR, &x)) return -1;
		if (x & DSCR_HALTED) goto halted;
	}
	fprintf(stderr, "v7debug: halt timed out\n");

halted:
	dwr(debug, DBGDSCR, DSCR_ITR_EN);

	dexec(debug, ARM_DSB); // dsb
	for (n = 0; n < 15; n++) {
		dexec(debug, ARM_MOV_DCC_Rx(n));
		dccrd(debug, r + n);
	}
	dexec(debug, ARM_MOV_R0_PC);
	dexec(debug, ARM_MOV_DCC_Rx(0));
	dccrd(debug, r + 15);
	dexec(debug, ARM_MOV_R0_CPSR);
	dexec(debug, ARM_MOV_DCC_Rx(0));
	dccrd(debug, &cpsr);
	for (n = 0; n < 16; n++) {
		fprintf(stderr,"R%d %08x  ", n, r[n]);
	}
	fprintf(stderr, "CPSR %08x\n", cpsr);
	return 0;
}

#define ZYNQ_DEBUG0_APN		1
#define ZYNQ_DEBUG0_BASE	0x80090000
#define ZYNQ_DEBUG1_APN		1
#define ZYNQ_DEBUG1_BASE	0x80092000

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;
	V7DEBUG *debug;

	if (jtag_mpsse_open(&jtag)) return -1;
	if ((dap = dap_init(jtag, 0x4ba00477)) == NULL) return -1;
	if ((debug = debug_init(dap, ZYNQ_DEBUG0_APN, ZYNQ_DEBUG0_BASE)) == NULL) return -1;
	if (debug_attach(debug)) return -1;

	fprintf(stderr, "whee!\n");
	return 0;
}

