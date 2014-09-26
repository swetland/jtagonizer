
// ARM Debug Interface

// See: Cortex R5 r1p2 TRM
// http://infocenter.arm.com/help/topic/com.arm.doc.ddi0460d/DDI0460D_cortex_r5_r1p2_trm.pdf

#define DBGDIDR		0x000 // Debug ID Register
#define DBGWFAR		0x018 // Watchpoint Fault Address Register
#define DBGVCR		0x01C // Vector Catch Register
#define DBGECR		0x024 // ?
#define DBGDSCCR	0x028 // Debug State Cache Control Register
#define DBGDSMCR	0x02C // ?
#define DBGDTRRX	0x080 // Data Transfer Register (host->target)
#define DBGITR		0x084 // Instruction Transfer Register
#define DBGDSCR		0x088 // Debug Status and Control Register
#define DBGDTRTX	0x08C // Data Transfer Register (target->host)
#define DBGDRCR		0x090 // Debug Run Control Register
#define DBGPCSR		0x0A0 // ?
#define DBGCIDSR	0x0A4 // ?
#define DBGBVR		0x100 // Breakpoint Value Registers
#define DBGBCR		0x140 // Breakpoint Control Registers
#define DBGWVR		0x180 // Watchpoint Value Registers
#define DBGWCR		0x1C0 // Watchpoint Control Registers
#define DBGOSLAR	0x300 // ?
#define DBGOSLSR	0x304 // Operating System Lock Status Register
#define DBGOSSRR	0x308 // ?
#define DBGPRCR		0x310 // Device Power-Down and Reset Control Register
#define DBGPRSR		0x314 // Device Power-Down and Reset Status Register
#define DBGLAR		0xFB0 // Lock Access Register
#define DBGLSR		0xFB4 // Lock Status Register
#define DBGAUTHSTATUS	0xDB8 // Authentication Status Register
#define DBGDEVID	0xFC8 // Device ID
#define DBGDEVTYPE	0xFCC // Device Type
#define DBGPID0		0xFD0
#define DBGCID0		0xFF0


#define CID_ROM_TABLE	0xB105100D
#define CID_CORESIGHT	0xB105900D

// See: A9 TRM
#define PID0_DEBUG	0x000BBC09

// See: Zynq TRM
#define PID0_CTI	0x002BB906
#define PID0_PTM	0x001BB950
#define PID0_ETB	0x003BB907
#define PID0_FTM	0x000C9001
#define PID0_FUNNEL	0x001BB908
#define PID0_ITM	0x002BB913
#define PID0_TPIU	0x004BB912
// 0x000BB4A9
// 0x000BB9A0
// 0x021893B2
