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

#include "dap.h"

int main(int argc, char **argv) {
	JTAG *jtag;
	DAP *dap;

	if (argc < 2) return -1;

	if (jtag_mpsse_open(&jtag)) return -1;
	if ((dap = dap_init(jtag, 0x4ba00477)) == NULL) return -1;
	if (dap_attach(dap)) return -1;

	if (argc == 2) {
		u32 x;
		if (dap_mem_rd32(dap, 0, strtoul(argv[1], 0, 0), &x)) return -1;
		printf("%08x\n", x);
	} else if (argc == 3) {
		return dap_mem_wr32(dap, 0, strtoul(argv[1], 0, 0), strtoul(argv[2], 0, 0));
	} else {
		return -1;
	}	
	return 0;
}

