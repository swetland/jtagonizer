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

int main(int argc, char **argv) {
	JTAG *jtag;
	u8 *data;
	u32 sz, n;

	if (argc != 2) {
		fprintf(stderr, "usage: fpga <bitfile>\n");
		return -1;
	}

	if ((data = loadfile(argv[1], &sz)) == NULL) return -1;

	for (n = 0; n < sz; n++) {
		data[n] = bitrev(data[n]);
	}

	if (jtag_mpsse_open(&jtag)) return -1;
	if (jtag_enumerate(jtag) < 0) return -1;
	if (jtag_select_by_family(jtag, "Xilinx 7")) return -1;

	fprintf(stderr, "begin.\n");
	n = 5;
	jtag_ir_wr(jtag, 6, &n);
	jtag_dr_wr(jtag, sz * 8, data);

	if (jtag_commit(jtag)) return -1;

	fprintf(stderr, "done.\n");
	return 0;
}

