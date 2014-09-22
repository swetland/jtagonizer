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

#ifndef _JTAG_
#define _JTAG_

#define JTAG_RESET	0
#define JTAG_IDLE	1
#define JTAG_DRSELECT	2
#define JTAG_DRCAPTURE	3
#define JTAG_DRSHIFT	4
#define JTAG_DREXIT1	5
#define JTAG_DRPAUSE	6
#define JTAG_DREXIT2	7
#define JTAG_DRUPDATE	8
#define JTAG_IRSELECT	9
#define JTAG_IRCAPTURE	10
#define JTAG_IRSHIFT	11
#define JTAG_IREXIT1	12
#define JTAG_IRPAUSE	13
#define JTAG_IREXIT2	14
#define JTAG_IRUPDATE	15

typedef struct JTAG JTAG;

int jtag_mpsse_open(JTAG **jtag);

void jtag_close(JTAG *jtag);

int jtag_setspeed(JTAG *jtag, int khz);

// which state to arrive at upon completion of jtag_ir_*()
void jtag_set_ir_idle(JTAG *jtag, unsigned state);

// which state to arrive at upon completion of jtag_dr_*()
void jtag_set_dr_idle(JTAG *jtag, unsigned state);

// bits to shift in ahead/behind the args for jtag_xr_*()
void jtag_set_ir_prefix(JTAG *jtag, unsigned count, const void *bits);
void jtag_set_ir_postfix(JTAG *jtag, unsigned count, const void *bits);
void jtag_set_dr_prefix(JTAG *jtag, unsigned count, const void *bits);
void jtag_set_dr_postfix(JTAG *jtag, unsigned count, const void *bits);

// Move jtag state machine from current state to new state.
// Moving to JTAG_RESET will work even if current state
// is out of sync.
void jtag_goto(JTAG *jtag, unsigned state);

// Move to IRSHIFT, shift count bits, then move to after_ir state.
void jtag_ir_wr(JTAG *jtag, unsigned count, const void *wbits);
void jtag_ir_rd(JTAG *jtag, unsigned count, void *rbits);
void jtag_ir_io(JTAG *jtag, unsigned count, const void *wbits, void *rbits);

// Move to DRSHIFT, shift count bits, then move to after_dr state;
void jtag_dr_wr(JTAG *jtag, unsigned count, const void *wbits);
void jtag_dr_rd(JTAG *jtag, unsigned count, void *rbits);
void jtag_dr_io(JTAG *jtag, unsigned count, const void *wbits, void *rbits);

int jtag_commit(JTAG *jtag);

#endif
