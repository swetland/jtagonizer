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

#include "dap.h"

u32 DATA[1024*1024];

int dap_probe(DAP *dap);

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;
	unsigned n;
	u32 x;

	if (jtag_mpsse_open(&jtag)) return -1;

	dap = dap_init(jtag, 0x4ba00477);
	if (dap_attach(dap))
		return -1;

//	dap_dp_rd(dap, DPACC_CSW, &x);
//	printf("DPCSW=%08x\n", x);

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
