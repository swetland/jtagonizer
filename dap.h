
#ifndef _DAP_H_
#define _DAP_H_

#include "jtag.h"

typedef struct DAP DAP;

// single word io -- must be 32bit aligned
int dap_mem_wr32(DAP *dap, u32 n, u32 addr, u32 val);
int dap_mem_rd32(DAP *dap, u32 n, u32 addr, u32 *val);

// multi-word io -- must be 32bit aligned
// len in bytes, must be 32bit aligned
int dap_mem_read(DAP *dap, u32 apnum, u32 addr, void *data, u32 len);
int dap_mem_write(DAP *dap, u32 apnum, u32 addr, void *data, u32 len);

int dap_attach(DAP *dap); 

DAP *dap_init(JTAG *jtag, u32 jtag_device_id);

int dap_attach(DAP *dap);

#endif
