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

#include <unistd.h>
#include <fcntl.h>

#include "jtag.h"

// See: UG470 Xilinx 7 Series FPGAs Configuration

// 7-Series JTAG IR
#define IR_LEN		6
#define IR_CFG_OUT	4
#define IR_CFG_IN	5
#define IR_JSHUTDOWN	13

// Config Packet Headers
#define CFG_DUMMY	0xFFFFFFFF
#define CFG_SYNC	0xAA995566
#define CFG_NOP		0x20000000
#define CFG_RD(addr,n)	(0x28000000 | ((addr)<<13) | (n))
#define CFG_WR(addr,n)	(0x30000000 | ((addr)<<13) | (n))

// Config Registers
#define CR_CRC		0
#define CR_FAR		1
#define CR_FDRI		2
#define CR_FDRO		3
#define CR_CMD		4
#define CR_CTL0		5
#define CR_MASK		6
#define CR_STAT		7
#define CR_LOUT		8
#define CR_COR0		9
#define CR_MFWR		10
#define CR_CBC		11
#define CR_IDCODE	12
#define CR_AXSS		13
#define CR_COR1		14
#define CR_WBSTAR	16
#define CR_TIMER	17
#define CR_BOOTSTS	22
#define CR_CTL1		24

// CR_CMD commands
#define CMD_NULL	0
#define CMD_WCFG	1
#define CMD_MFW		2
#define CMD_LFRM	3
#define CMD_RCFG	4
#define CMD_START	5
#define CMD_RCAP	6
#define CMD_RCRC	7
#define CMD_AGHIGH	8
#define CMD_SWITCH	9
#define CMD_GRESTORE	10
#define CMD_SHUTDOWN	11
#define CMD_GCAPTURE	12
#define CMD_DESYNC	13
#define CMD_IPROG	15
#define CMD_CRCC	16
#define CMD_LTIMER	17

// CR_STAT bits
#define STAT_DEC_ERROR		(1 << 16)
#define STAT_ID_ERROR		(1 << 15)
#define STAT_DONE		(1 << 14)
#define STAT_RELEASE_DONE	(1 << 13)
#define STAT_INIT_B		(1 << 12)
#define STAT_INIT_COMPLETE	(1 << 11)
#define STAT_GHIGH_B		(1 << 7)
#define STAT_GWE		(1 << 6)
#define STAT_GTS_CFG_B		(1 << 5)	// 0 = IOs are HIGH-Z
#define STAT_EOS		(1 << 4)	// EndOfStartup 
#define STAT_DCI_MATCH		(1 << 3)
#define STAT_MMCM_LOCK		(1 << 2)	// all used MMCMs have locked
#define STAT_PART_SECURED	(1 << 1)
#define STAT_CRC_ERROR		(1 << 0)

#define STAT_STATE(n)	(((n) >> 18) & 7)
#define STAT_MODE(n)	(((n) >> 8) & 7)

static void *loadfile(const char *fn, u32 *sz) {
	int fd;
	off_t end;
	void *data = NULL;
	if ((fd = open(fn, O_RDONLY)) < 0) return NULL;
	if ((end = lseek(fd, 0, SEEK_END)) < 0) goto oops;
	if (lseek(fd, 0, SEEK_SET) < 0) goto oops;
	if ((data = malloc(end + 4)) == NULL) goto oops;
	if (read(fd, data, end) != end) goto oops;
	close(fd);
	*sz = end;
	return data;

oops:
	free(data);
	close(fd);
	return NULL;
}

static u8 bitrev(u8 x) {
	x = (x << 4) | (x >> 4);
	x = ((x << 2) & 0xcc) | ((x >> 2) & 0x33);
	x = ((x << 1) & 0xaa) | ((x >> 1) & 0x55);
	return x;
}

static void swizzle(u32 *_data, int count) {
	u8 *data = (void*) _data;
	u8 a, b, c, d;
	while (count-- > 0) {
		a = data[0];
		b = data[1];
		c = data[2];
		d = data[3];
		data[0] = bitrev(d);
		data[1] = bitrev(c);
		data[2] = bitrev(b);
		data[3] = bitrev(a);
		data += 4;
	}
}

static int txn(JTAG *jtag, u32 *send, int scount, u32 *recv, int rcount) {
	u32 n;

	swizzle(send, scount);

	jtag_goto(jtag, JTAG_RESET);
	n = IR_CFG_IN;
	jtag_ir_wr(jtag, IR_LEN, &n);
	jtag_dr_wr(jtag, 32 * scount, send);
	if (rcount) {
		n = IR_CFG_OUT;
		jtag_ir_wr(jtag, IR_LEN, &n);
		jtag_dr_rd(jtag, 32 * rcount, recv);
		jtag_goto(jtag, JTAG_RESET);
	}

	if (jtag_commit(jtag)) {
		return -1;
	} else {
		swizzle(recv, rcount);
		return 0;
	}
}

static int fpga_rd_status(JTAG *jtag, u32 *status) {
	u32 tx[5];
	tx[0] = CFG_SYNC;
	tx[1] = CFG_NOP;
	tx[2] = CFG_RD(CR_STAT, 1);
	tx[3] = CFG_NOP;
	tx[4] = CFG_NOP;
	return txn(jtag, tx, 5, status, 1);
}

static int fpga_warm_boot(JTAG *jtag) {
	u32 tx[8];

	tx[0] = CFG_SYNC;
	tx[1] = CFG_NOP;
	tx[2] = CFG_WR(CR_CMD, 1);
	tx[3] = CMD_IPROG;
	tx[4] = CFG_WR(CR_FAR, 1);
	tx[5] = 0;
	tx[6] = CFG_NOP;
	tx[7] = CFG_NOP;

	return txn(jtag, tx, 8, NULL, 0);

#if 0
	n = IR_JSHUTDOWN;
	jtag_ir_wr(jtag, IR_LEN, &n);
	jtag_idle(jtag, 12);
	return jtag_commit(jtag);
#endif
}

int main(int argc, char **argv) {
	JTAG *jtag;
	u8 *data = NULL;
	u32 sz, n;

	if (argc == 2) {
		if ((data = loadfile(argv[1], &sz)) == NULL) {
			fprintf(stderr, "error: cannot load '%s'\n", argv[1]);
			return -1;
		}
		for (n = 0; n < sz; n++) {
			data[n] = bitrev(data[n]);
		}
	}

	if (jtag_mpsse_open(&jtag)) return -1;
	if (jtag_enumerate(jtag) < 0) return -1;
	if (jtag_select_by_family(jtag, "Xilinx 7")) return -1;

	n = 0;
	if (fpga_rd_status(jtag, &n)) {
		fprintf(stderr, "error: failed to read status\n");
		return -1;
	}
	fprintf(stderr, "status: %08x S%d\n", n, STAT_STATE(n));
	if (data == NULL) return 0;

	fpga_warm_boot(jtag);

	//TODO: detect ready via status register
	usleep(100000);

	n = 0;
	if (fpga_rd_status(jtag, &n)) {
		fprintf(stderr, "error: failed to read status\n");
		return -1;
	}
	fprintf(stderr, "status: %08x S%d\n", n, STAT_STATE(n));

	fprintf(stderr, "downloading...\n");
	jtag_goto(jtag, JTAG_RESET);
	n = IR_CFG_IN;
	jtag_ir_wr(jtag, IR_LEN, &n);
	jtag_dr_wr(jtag, sz * 8, data);

	if (jtag_commit(jtag)) return -1;

	n = 0;
	if (fpga_rd_status(jtag, &n)) {
		fprintf(stderr, "failed to read status\n");
		return -1;
	}
	if (n & STAT_CRC_ERROR) {
		fprintf(stderr, "error: bitstream CRC error\n");
		return -1;
	}
	if (n & STAT_ID_ERROR) {
		fprintf(stderr, "error: bitstream part ID does not match\n");
		return -1;
	}
	if (n & STAT_INIT_COMPLETE) {
		fprintf(stderr, "init complete\n");
	}

	return 0;
}

