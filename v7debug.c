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

#include "v7debug-registers.h"

// CPSR bits
#define ARM_N		(1 << 31)
#define ARM_Z		(1 << 30)
#define ARM_C		(1 << 29)
#define ARM_V		(1 << 28)
#define ARM_Q		(1 << 27)
#define ARM_J		(1 << 24)
#define ARM_E		(1 << 9)
#define ARM_A		(1 << 8)
#define ARM_I		(1 << 7)
#define ARM_F		(1 << 6)
#define ARM_T		(1 << 5)
#define ARM_M_MASK	0x1F
#define ARM_M_USR	0x10
#define ARM_M_FIQ	0x11
#define ARM_M_IRQ	0x12
#define ARM_M_SVC	0x13
#define ARM_M_MON	0x16
#define ARM_M_ABT	0x17
#define ARM_M_UND	0x1B
#define ARM_M_SYS	0x1F

#define STATE_IDLE	0
#define STATE_HALTED	1
#define STATE_RUNNING	2

struct V7DEBUG {
	DAP *dap;
	unsigned state;
	u32 apnum;
	u32 base;

	// saved state while halted
	u32 save_r0;
	u32 save_r1;
	u32 save_pc;
	u32 save_cpsr;

	// cached cpsr
	u32 cpsr;
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

static int dccrd(V7DEBUG *debug, u32 *val) {
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

static int dccwr(V7DEBUG *debug, u32 val) {
	u32 x;
	int n;
	for (n = 0; n < 10; n++) {
		if (drd(debug, DBGDSCR, &x)) return -1;
		if (!(x & DSCR_RXFULL)) {
			return dwr(debug, DBGDTRRX, val);
		}
	}
	fprintf(stderr, "v7debug: dcc write timed out\n");
	return -1;
}

static int dexec(V7DEBUG *debug, u32 instr) {
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

#define ARM_DSB			0xEE070F9A
#define ARM_ICIALLU		0xEE070F15
#define ARM_ISB			0xEE070F95
#define ARM_MOV_DCC_Rx(x)	(0xEE000E15 | ((x) << 12)) // Rx -> DCC
#define ARM_MOV_Rx_DCC(x)	(0xEE100E15 | ((x) << 12)) // DCC -> Rx
#define ARM_MOV_R0_PC		0xE1A0000F
#define ARM_MOV_PC_R0		0xE1A0F000
//#define ARM_MOV_CPSR_R0		0xE12FF000 // R0 -> CPSR
#define ARM_MOV_CPSR_R0		0xE129F000 // R0 -> CPSR
#define ARM_MOV_R0_CPSR		0xE10F0000 // CPSR -> R0

int debug_reg_rd(V7DEBUG *debug, unsigned n, u32 *val) {
	if (debug->state != STATE_HALTED) {
		return -1;
	}
	switch (n) {
	case 0: { *val = debug->save_r0; return 0; }
	case 1: { *val = debug->save_r1; return 0; }
	case 15: { *val = debug->save_pc; return 0; }
	case 16: { *val = debug->save_cpsr; return 0; }
	}
	if (n > 15) {
		return -1;
	}
	if (dexec(debug, ARM_MOV_DCC_Rx(n))) return -1;
	return dccrd(debug, val);
}

int debug_reg_wr(V7DEBUG *debug, unsigned n, u32 val) {
	if (debug->state != STATE_HALTED) {
		return -1;
	}
	switch (n) {
	case 0: { debug->save_r0 = val; return 0; }
	case 1: { debug->save_r1 = val; return 0; }
	case 15: { debug->save_pc = val; return 0; }
	case 16: { debug->save_cpsr = val; return 0; }
	}
	if (n > 15) {
		return -1;
	}
	if (dccwr(debug, val)) return -1;
	return dexec(debug, ARM_MOV_Rx_DCC(n));
}

int debug_attach(V7DEBUG *debug) {
	u32 x;
	int n;

	if (debug->state == STATE_HALTED) {
		return -1;
	}

	drd(debug, DBGDSCR, &x);
	if (x & DSCR_HALTED) {
		fprintf(stderr, "debug: warning, processor already halted\n");
	}
		
	dwr(debug, DBGDSCR, DSCR_H_DBG_EN | DSCR_RESTARTED | DSCR_HALTED);

	dwr(debug, DBGDRCR, DRCR_HALT_REQ);
	for (n = 0; n < 100; n++) {
		if (drd(debug, DBGDSCR, &x)) return -1;
		if (x & DSCR_HALTED) goto halted;
	}
	fprintf(stderr, "v7debug: halt timed out\n");
	return -1;

halted:
	dwr(debug, DBGDSCR, DSCR_H_DBG_EN | DSCR_ITR_EN | DSCR_RESTARTED | DSCR_HALTED);

	// save essential state
	// we need r0/r1 to shuffle data in/out of memory and dcc
	// pc will be corrupted on cpsr writes
	// cpsr needs to be written to access other modes
	dexec(debug, ARM_DSB);
	dexec(debug, ARM_MOV_DCC_Rx(0));
	dccrd(debug, &debug->save_r0);
	dexec(debug, ARM_MOV_DCC_Rx(1));
	dccrd(debug, &debug->save_r1);
	dexec(debug, ARM_MOV_R0_PC);
	dexec(debug, ARM_MOV_DCC_Rx(0));
	dccrd(debug, &debug->save_pc);
	dexec(debug, ARM_MOV_R0_CPSR);
	dexec(debug, ARM_MOV_DCC_Rx(0));
	dccrd(debug, &debug->save_cpsr);
	if (debug->save_cpsr & ARM_T) {
		debug->save_pc -= 4;
	} else {
		debug->save_pc -= 8;
	}
	debug->cpsr = debug->save_cpsr;
	debug->state = STATE_HALTED;
	return 0;
}

int debug_detach(V7DEBUG *debug) {
	if (debug->state != STATE_HALTED) {
		return -1;
	}

	dccwr(debug, debug->save_cpsr);
	dexec(debug, ARM_MOV_Rx_DCC(0));
	dexec(debug, ARM_MOV_CPSR_R0);

	dccwr(debug, debug->save_pc);
	dexec(debug, ARM_MOV_Rx_DCC(0));
	dexec(debug, ARM_MOV_PC_R0);

	dccwr(debug, debug->save_r0);
	dexec(debug, ARM_MOV_Rx_DCC(0));

	dccwr(debug, debug->save_r1);
	dexec(debug, ARM_MOV_Rx_DCC(1));

	dexec(debug, ARM_ICIALLU);
	dexec(debug, ARM_ISB);

	dwr(debug, DBGDSCR, 0);
	dwr(debug, DBGDRCR, DRCR_CLR_EXC);
	dwr(debug, DBGDRCR, DRCR_START_REQ);

	debug->state = STATE_RUNNING;
	return 0;
}

int debug_reg_dump(V7DEBUG *debug) {
	int n;
	u32 r[17];
	for (n = 0; n < 17; n++) {
		if (debug_reg_rd(debug, n, r + n)) return -1;
	}

	printf("  r0: %08x  r1: %08x  r2: %08x  r3: %08x\n",
		r[0], r[1], r[2], r[3]);
	printf("  r4: %08x  r5: %08x  r6: %08x  r7: %08x\n",
		r[4], r[5], r[6], r[7]);
	printf("  r8: %08x  r9: %08x r10: %08x r11: %08x\n",
		r[8], r[9], r[10], r[11]);
	printf(" r12: %08x  sp: %08x  lr: %08x  pc: %08x\n",
		r[12], r[13], r[14], r[15]);
	printf("cpsr: %08x\n", r[16]);
	return 0;
}
