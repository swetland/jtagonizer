#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jtag.h"

int main(int argc, char **argv) {
	unsigned ir, x;
	unsigned p0 = 0x80808080;
	unsigned p1 = 0x12345678;
	unsigned p2 = 0xaabbccdd;
	unsigned x0, x1, x2;
	unsigned char b0[8192], b1[8192];
	unsigned n, i;
	unsigned char C;
	JTAG *jtag;

	if (jtag_mpsse_open(&jtag)) {
		fprintf(stderr, "error opening jtag\n");
		return -1;
	}

	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_commit(jtag);

	ir = 9;
	jtag_set_dr_idle(jtag, JTAG_DRPAUSE);
	jtag_goto(jtag, JTAG_RESET);
	jtag_ir_wr(jtag, 6, &ir);
	jtag_dr_io(jtag, 32, &p0, &x);
	jtag_dr_io(jtag, 32, &p1, &x0);
	jtag_dr_io(jtag, 32, &p1, &x1);
	jtag_dr_wr(jtag, 32, &p2);
	jtag_dr_rd(jtag, 32, &x2);
	jtag_commit(jtag);

	fprintf(stderr,"%x %x %x %x\n", x, x0, x1, x2);

#if 1 
	for (i = 0; i < 1000; i++) {
	C = i;
#define XX 8000
	memset(b0,C,XX);
	//for (n = 0; n < 256; n++) b0[n] = n;
	jtag_goto(jtag, JTAG_RESET);
	jtag_goto(jtag, JTAG_RESET);
	jtag_ir_wr(jtag, 6, &ir);
	jtag_dr_io(jtag, XX*8, b0, b1);
	jtag_commit(jtag);

	if ((b1[0] != 0x93) || (b1[1] != 0x10) || (b1[2] != 0x63) || (b1[3] != 0x13)) {
		fprintf(stderr,"OOPS\n");
		for (n = 0; n < XX; n++) printf("%02x ", b1[n]);
		return -1;
	}
	for (n = 4; n < XX; n++) {
		if (b1[n] != C) {
			fprintf(stderr,"OOPS @ %d\n", n);
			for (n = 0; n < XX; n++) printf("%02x ", b1[n]);
			return -1;
		}
	}
	}
	fprintf(stderr,"success!\n");
#endif
	return 0;
}
