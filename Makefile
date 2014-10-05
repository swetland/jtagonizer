
CFLAGS := -Wall -O0 -g 

LIBS := -lusb-1.0 -lrt


all: zynq fpga

JTAG_OBJS := jtag-mpsse-driver.o jtag-core.o jtag.o
$(JTAG_OBJS): jtag.h jtag-driver.h
jtag: $(JTAG_OBJS)
	$(CC) -o jtag $(JTAG_OBJS) $(LIBS)

DAP_OBJS := dap-test.o dap.o jtag-core.o jtag-mpsse-driver.o
$(DAP_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h
dap-test: $(DAP_OBJS)
	$(CC) -o dap-test $(DAP_OBJS) $(LIBS)

ZYNQ_OBJS := zynq.o v7debug.o dap.o jtag-core.o jtag-mpsse-driver.o
$(ZYNQ_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h v7debug.h v7debug-registers.h
zynq: $(ZYNQ_OBJS)
	$(CC) -o zynq $(ZYNQ_OBJS) $(LIBS)

FPGA_OBJS := fpga.o jtag-core.o jtag-mpsse-driver.o
$(FPGA_OBJS): jtag.h jtag-driver.h
fpga: $(FPGA_OBJS)
	$(CC) -o fpga $(FPGA_OBJS) $(LIBS)

clean:
	rm -f *.o jtag dap-test zynq fpga
