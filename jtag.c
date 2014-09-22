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
	JTAG *jtag;

	if (jtag_mpsse_open(&jtag)) {
		fprintf(stderr, "error opening jtag\n");
		return -1;
	}

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

	return 0;
}
