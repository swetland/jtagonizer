
CFLAGS := -Wall -O0 -g 

LIBS := -lusb-1.0 -lrt


all: jtag dap-test debug

JTAG_OBJS := jtag-mpsse-driver.o jtag-core.o jtag.o
$(JTAG_OBJS): jtag.h jtag-driver.h
jtag: $(JTAG_OBJS)
	$(CC) -o jtag $(JTAG_OBJS) $(LIBS)

DAP_OBJS := dap-test.o dap.o jtag-core.o jtag-mpsse-driver.o
$(DAP_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h
dap-test: $(DAP_OBJS)
	$(CC) -o dap-test $(DAP_OBJS) $(LIBS)

DEBUG_OBJS := v7debug.o dap.o jtag-core.o jtag-mpsse-driver.o
$(DEBUG_OBJS): jtag.h jtag-driver.h dap.h dap-registers.h v7debug.h
debug: $(DEBUG_OBJS)
	$(CC) -o debug $(DEBUG_OBJS) $(LIBS)

clean:
	rm -f *.o jtag dap-test debug
