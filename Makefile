
CFLAGS := -Wall -O0 -g 

LIBS := -lusb-1.0 -lrt

all: zynq debug mem

JTAG_OBJS := jtag-mpsse-driver.o jtag-core.o jtag.o
$(JTAG_OBJS): jtag.h jtag-driver.h
jtag: $(JTAG_OBJS)
	$(CC) -o jtag $(JTAG_OBJS) $(LIBS)

DAP_OBJS := dap-test.o dap.o jtag-core.o jtag-mpsse-driver.o
$(DAP_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h
dap-test: $(DAP_OBJS)
	$(CC) -o dap-test $(DAP_OBJS) $(LIBS)

ZYNQ_OBJS := zynq.o fpga.o v7debug.o dap.o jtag-core.o jtag-mpsse-driver.o
$(ZYNQ_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h v7debug.h v7debug-registers.h
zynq: $(ZYNQ_OBJS)
	$(CC) -o zynq $(ZYNQ_OBJS) $(LIBS)

DEBUG_OBJS := debug.o jtag-core.o jtag-mpsse-driver.o
$(DEBUG_OBJS): jtag.h jtag-driver.h
debug: $(DEBUG_OBJS)
	$(CC) -o debug $(DEBUG_OBJS) $(LIBS)

MEM_OBJS := mem.o dap.o jtag-core.o jtag-mpsse-driver.o
$(MEM_OBJS): dap.h jtag.h jtag-driver.h
mem: $(MEM_OBJS)
	$(CC) -o mem $(MEM_OBJS) $(LIBS)

clean:
	rm -f *.o jtag dap-test zynq debug mem
