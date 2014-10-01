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

#include "v7debug.h"

#define ZYNQ_DEBUG0_APN		1
#define ZYNQ_DEBUG0_BASE	0x80090000
#define ZYNQ_DEBUG1_APN		1
#define ZYNQ_DEBUG1_BASE	0x80092000

void *loadfile(const char *fn, u32 *sz) {
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

int usage(void) {
	fprintf(stderr,
"zynq run <image>              download image to 0, resume cpu0 at 0\n"
"zynq regs                     pause both cpus, dump registers, resume\n"
"\n"
		);
	return -1;
}

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;
	V7DEBUG *d0;
	V7DEBUG *d1;
	void *data;
	u32 sz;

	if (argc < 2) {
		return usage();
	}

	if (jtag_mpsse_open(&jtag)) return -1;
	if ((dap = dap_init(jtag, 0x4ba00477)) == NULL) return -1;
	if (dap_attach(dap)) return -1;
	if ((d0 = debug_init(dap, ZYNQ_DEBUG0_APN, ZYNQ_DEBUG0_BASE)) == NULL) return -1;
	if ((d1 = debug_init(dap, ZYNQ_DEBUG1_APN, ZYNQ_DEBUG1_BASE)) == NULL) return -1;

	if (!strcmp(argv[1], "run")) {
		if (argc != 3) {
			return usage();
		}
		if ((data = loadfile(argv[2], &sz)) == NULL) {
			fprintf(stderr, "error: could not load '%s'\n", argv[2]);
			return -1;
		}
		if (sz > (192*1024)) {
			fprintf(stderr, "error: image too large\n");
			return -1;
		}
		if (debug_attach(d0)) return -1;
		dap_mem_write(dap, 0, 0, data, sz);
		debug_reg_wr(d0, 15, 0);
		debug_detach(d0);
	} else if (!strcmp(argv[1], "regs")) {
		debug_attach(d0);
		debug_attach(d1);
		printf("CPU0:\n");
		debug_reg_dump(d0);
		printf("\nCPU1:\n");
		debug_reg_dump(d1);
		debug_detach(d0);
		debug_detach(d1);
	} else if (!strcmp(argv[1], "reset")) {
		u32 x;
		dap_mem_wr32(dap, 0, 0xF8000008, 0xDF0D);
		dap_mem_wr32(dap, 0, 0xF8000200, 1);
	} else {
		return usage();
	}

	return 0;
}

