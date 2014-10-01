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

#ifndef _V7DEBUG_H_
#define _V7DEBUG_H_

#include "dap.h"

typedef struct V7DEBUG V7DEBUG;

V7DEBUG *debug_init(DAP *dap, u32 apnum, u32 base);

// attach and halt cpu
int debug_attach(V7DEBUG *debug);

// detach and resume cpu
int debug_detach(V7DEBUG *debug);

// only valid while attached
int debug_reg_rd(V7DEBUG *debug, unsigned n, u32 *val);
int debug_reg_wr(V7DEBUG *debug, unsigned n, u32 val);

int debug_reg_dump(V7DEBUG *debug);

#endif
