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

#ifndef _JTAG_DRIVER_
#define _JTAG_DRIVER_

#include "jtag.h"

typedef struct JDRV JDRV;

typedef struct {
	int (*init)(JDRV *d);

// returns actual speed
// if khz is negative, only query speed, do not set
	int (*setspeed)(JDRV *drv, int khz);

// Declare the end of a transaction, block until success or failure.
// Once this returns, pointers passed via scan_*() may become invalid.
	int (*commit)(JDRV *d);

// Shift count TMS=tbits TDI=obit out.
// If ibits is nonnull, capture the first TDO at offset ioffset in ibits.
	int (*scan_tms)(JDRV *d, u32 obit, u32 count, u8 *tbits,
			u32 ioffset, u8 *ibits);

// Shift count bits.
// If obits nonnull, shift out those bits to TDI.
// If ibits nonnull, capture to those bits from TDO.
// TMS does not change.
	int (*scan_io)(JDRV *d, u32 count, u8 *obits, u8 *ibits);

// Close and release driver.
	int (*close)(JDRV *d);
} JDVT;

int jtag_init(JTAG **jtag, JDRV *drv, JDVT *vt);

#endif


