
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

#define DSCR_RXFULL	(1 << 31)	// (cleared on read)
#define DSCR_TXFULL	(1 << 30)	// (cleared on read)
#define DSCR_PIPEADV	(1 << 25)	// (cleared on DRCR write) set on instr exec
#define DSCR_INSTRCOMPL	(1 << 24)	// 0 if cpu executing from ITR
#define DSCR_DCC_MASK	(3 << 20)	// DTR access mode
#define DSCR_DCC_NBLOCK	(0 << 20)
#define DSCR_DCC_STALL	(1 << 20)
#define DSCR_DCC_FAST	(2 << 20)
#define DSCR_M_DBG_EN	(1 << 15)	// monitor debug enabled
#define DSCR_H_DBG_EN	(1 << 14)	// halting debug enabled
#define DSCR_ITR_EN	(1 << 13)	// instruction execution enabled
#define DSCR_UDCC_DIS	(1 << 12)	// CP14 user debug disable
#define DSCR_INT_DIS	(1 << 11)	// interrupts disable
#define DSCR_DBG_ACK	(1 << 10)	// force DBGACKm signal HIGH
#define DSCR_UND	(1 << 8)	// sticky undefined bit (wtc)
#define DSCR_ADABORT	(1 << 7)	// sticky asynch data abort bit (wtc)
#define DSCR_SDABORT	(1 << 6)	// sticky synch data abort bit (wtc)
#define DSCR_M_MASK	(15 << 2)	// method-of-entry mask
#define DSCR_M_HALT	(0 << 2)	// halt from DRCR[0]
#define DSCR_M_BKPT	(1 << 2)	// breakpoint
#define DSCR_M_EDBGRQ	(4 << 2)
#define DSCR_M_BKPT_INS	(3 << 2)	// breakpoint instruction
#define DSCR_M_WATCHPT	(10 << 2)	// synchronous watchpoint
#define DSCR_RESTARTED	(1 << 1)	// 1 = not in debug state
#define DSCR_HALTED	(1 << 0)	// 1 = in debug state

#define VCR_FIQ		(1 << 7)
#define VCR_IRQ		(1 << 6)
#define VCR_DABORT	(1 << 4)
#define VCR_PABORT	(1 << 3)
#define VCR_SVC		(1 << 2)
#define VCR_UND		(1 << 1)
#define VCR_RESET	(1 << 0)

#define DSCCR_WT_EN	(1 << 2)	// 0 = force write through in debug state
#define DSCCR_IL_EN	(1 << 1)	// 0 = L1 icache line-fills disabled in debug
#define DSCCR_DL_EN	(1 << 0)	// 0 = L1 dcache line-fills disabled in debug

#define DRCR_CANCEL	(1 << 4)	// cancel memory request
					// processor will abandon inflight mem reqs
#define DRCR_PIPEADV	(1 << 3)	// clear DSCR_PIPEADV
#define DRCR_CLR_EXC	(1 << 2)	// clear sticky exceptions
#define DRCR_START_REQ	(1 << 1)	// request processor restart
#define DRCR_HALT_REQ	(1 << 0)	// request processor halt

#define PRSR_S_RESET	(1 << 3)	// sticky reset status
#define PRSR_RESET	(1 << 2)	// reset status
#define PRSR_S_PDOWN	(1 << 1)	// sticky power-down status
#define PRSR_PDOWN	(1 << 0)	// power-down status

#define LAR_KEY		0xC5ACCE55	// write to LAR to enable debug regs

#define LSR_32BIT	(1 << 2)
#define LSR_LOCKED	(1 << 1)
#define LSR_LOCK_IMPL	(1 << 0)

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
