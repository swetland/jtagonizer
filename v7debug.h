
// ARM v7 Debug Interface

// See: Cortex R5 r1p2 TRM
// http://infocenter.arm.com/help/topic/com.arm.doc.ddi0460d/DDI0460D_cortex_r5_r1p2_trm.pdf

#define V7DBGDIDR	0x000 // Debug ID Register
#define V7DBGWFAR	0x018 // Watchpoint Fault Address Register
#define V7DBGVCR	0x01C // Vector Catch Register
#define V7DBGECR	0x024 // ?
#define V7DBGDSCCR	0x028 // Debug State Cache Control Register
#define V7DBGDSMCR	0x02C //XXX
#define V7DBGDTRRX	0x080 // Data Transfer Register (host->target)
#define V7DBGITR	0x084 // Instruction Transfer Register
#define V7DBGDSCR	0x088 // Debug Status and Control Register
#define V7DBGDTRTX	0x08C // Data Transfer Register (target->host)
#define V7DBGDRCR	0x090 // Debug Run Control Register
#define V7DBGPCSR	0x0A0 // ?
#define V7DBGCIDSR	0x0A4 // ?
#define V7DBGBVR	0x100 // Breakpoint Value Registers
#define V7DBGBCR	0x140 // Breakpoint Control Registers
#define V7DBGWVR	0x180 // Watchpoint Value Registers
#define V7DBGWCR	0x1C0 // Watchpoint Control Registers
#define V7DBGOSLAR	0x300 // ?
#define V7DBGOSLSR	0x304 // Operating System Lock Status Register
#define V7DBGOSSRR	0x308 // ?
#define V7DBGPRCR	0x310 // Device Power-Down and Reset Control Register
#define V7DBGPRSR	0x314 // Device Power-Down and Reset Status Register
#define V7DBGLAR	0xFB0 // Lock Access Register
#define V7DBGLSR	0xFB4 // Lock Status Register
#define V7DBGAUTHSTATUS	0xDB8 // Authentication Status Register
#define V7DBGDEVID	0xFC8 // Device ID
#define V7DBGDEVTYPE	0xFCC // Device Type
#define V7DBGPID0	0xFD0
#define V7DBGCID0	0xFF0
