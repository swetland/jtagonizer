
CFLAGS := -Wall -O2 -g

LIBS := -lusb-1.0


all: jtag dap

JTAG_OBJS := jtag-mpsse-driver.o jtag-core.o jtag.o
$(JTAG_OBJS): jtag.h jtag-driver.h
jtag: $(JTAG_OBJS)
	$(CC) -o jtag $(JTAG_OBJS) $(LIBS)

DAP_OBJS := dap.o jtag-core.o jtag-mpsse-driver.o
$(DAP_OBJS): jtag.h jtag-driver.h dap.h
dap: $(DAP_OBJS)
	$(CC) -o dap $(DAP_OBJS) $(LIBS)

clean:
	rm -f *.o jtag dap
