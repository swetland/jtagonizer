
CFLAGS := -Wall -O2 -g

LIBS := -lusb-1.0


all: jtag

JTAG_OBJS := jtag-mpsse-driver.o jtag-core.o jtag.o

$(JTAG_OBJS): jtag.h

jtag: $(JTAG_OBJS)
	$(CC) -o jtag $(JTAG_OBJS) $(LIBS)

clean:
	rm -f *.o jtag
