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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jtag-driver.h"

#define TRACE_IO 1
#define TRACE_DISASM 1

// FTDI MPSSE Device Info
static struct {
	u16 vid;
	u16 pid;
	u8 ep_in;
	u8 ep_out;
	const char *name;
} devinfo[] = {
	{ 0x0403, 0x6010, 0x81, 0x02, "ftdi" },
	{ 0x0403, 0x6014, 0x81, 0x02, "ftdi" },
	{ 0x0000, 0x0000, 0},
};

static unsigned char mpsse_init[] = {
	0x85, // loopback off
	0x8a, // disable clock/5
	0x86, 0x02, 0x00, // set divisor
	0x80, 0xe8, 0xeb, // set low state and dir
	0x82, 0x00, 0x00, // set high state and dir
};

static void dump(char *prefix, void *data, int len) {
	unsigned char *x = data;
	fprintf(stderr,"%s: (%d)", prefix, len);
	while (len > 0) {
		fprintf(stderr," %02x", *x++);
		len--;
	}
	fprintf(stderr,"\n");
}

#include <libusb-1.0/libusb.h>

// how to process the reply buffer
#define OP_END		0 // done
#define OP_BITS		1 // copy top n (1-8) bits to ptr
#define OP_BYTES	2 // copy n (1-65536) bytes to ptr
#define OP_1BIT		3 // copy bitmask n to bitmask x of ptr

typedef struct {
	u8 *ptr;
	u32 n;
	u16 op;
	u16 x;
} JOP;

#define CMD_MAX (16*1024)

struct JDRV {
	struct libusb_device_handle *udev;
	u8 ep_in;
	u8 ep_out;
	int speed;
	u32 expected;
	u32 status;
	u8 *next;
	JOP *nextop;
	u32 read_count;
	u32 read_size;
	u8 *read_ptr;
	u8 cmd[CMD_MAX];
	JOP op[8192];
	u8 read_buffer[512];
};

static inline u32 cmd_avail(JDRV *d) {
	return CMD_MAX - (d->next - d->cmd);
}

static int _jtag_setspeed(JDRV *d, int khz) {
	return d->speed;
}

static void resetstate(JDRV *d) {
	d->status = 0;
	d->next = d->cmd;
	d->nextop = d->op;
	d->expected = 0;
}

#define FTDI_REQTYPE_OUT	(LIBUSB_REQUEST_TYPE_VENDOR \
	| LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT)
#define FTDI_CTL_RESET		0x00
#define FTDI_CTL_SET_BITMODE	0x0B
#define FTDI_CTL_SET_EVENT_CH	0x06
#define FTDI_CTL_SET_ERROR_CH	0x07

#define FTDI_IFC_A 1
#define FTDI_IFC_B 2

static int ftdi_reset(JDRV *d) {
	struct libusb_device_handle *udev = d->udev;
	if (libusb_control_transfer(udev, FTDI_REQTYPE_OUT, FTDI_CTL_RESET,
		0, FTDI_IFC_A, NULL, 0, 10000) < 0) {
		fprintf(stderr, "ftdi: reset failed\n");
		return -1;
	}
	return 0;
}

static int ftdi_mpsse_enable(JDRV *d) {
	struct libusb_device_handle *udev = d->udev;
	if (libusb_control_transfer(udev, FTDI_REQTYPE_OUT, FTDI_CTL_SET_BITMODE,
		0x0000, FTDI_IFC_A, NULL, 0, 10000) < 0) {
		fprintf(stderr, "ftdi: set bitmode failed\n");
		return -1;
	}
	if (libusb_control_transfer(udev, FTDI_REQTYPE_OUT, FTDI_CTL_SET_BITMODE,
		0x020b, FTDI_IFC_A, NULL, 0, 10000) < 0) {
		fprintf(stderr, "ftdi: set bitmode failed\n");
		return -1;
	}
	if (libusb_control_transfer(udev, FTDI_REQTYPE_OUT, FTDI_CTL_SET_EVENT_CH,
		0, FTDI_IFC_A, NULL, 0, 10000) < 0) {
		fprintf(stderr, "ftdi: disable event character failed\n");
		return -1;
	}
	return 0;	
	if (libusb_control_transfer(udev, FTDI_REQTYPE_OUT, FTDI_CTL_SET_ERROR_CH,
		0, FTDI_IFC_A, NULL, 0, 10000) < 0) {
		fprintf(stderr, "ftdi: disable error character failed\n");
		return -1;
	}
	return 0;	
}

static int ftdi_open(JDRV *d) {
	struct libusb_device_handle *udev;
	int n;

	if (libusb_init(NULL) < 0) {
		fprintf(stderr, "jtag_init: failed to init libusb\n");
		return -1;
	}
	for (n = 0; devinfo[n].name; n++) {
		udev = libusb_open_device_with_vid_pid(NULL,
			devinfo[n].vid, devinfo[n].pid);
		if (udev == 0)
	       		continue;
		libusb_detach_kernel_driver(udev, 0);
		if (libusb_claim_interface(udev, 0) < 0) {
			//TODO: close
			continue;
		}
		d->udev = udev;
		d->read_ptr = d->read_buffer;
		d->read_size = 512;
		d->read_count = 0;
		d->ep_in = devinfo[n].ep_in;
		d->ep_out = devinfo[n].ep_out;
		return 0;
	}
	fprintf(stderr, "jtag_init: failed to find usb device\n");
	return -1;
}

static int usb_bulk(struct libusb_device_handle *udev,
	unsigned char ep, void *data, int len, unsigned timeout) {
	int r, xfer;
	r = libusb_bulk_transfer(udev, ep, data, len, &xfer, timeout);
	if (r < 0) {
		fprintf(stderr,"bulk: error: %d\n", r);
		return r;
	}
	return xfer;
}

/* TODO: handle smaller packet size for lowspeed version of the part */
/* TODO: multi-packet reads */
/* TODO: asynch/background reads */
static int ftdi_read(JDRV *d, unsigned char *buffer, int count, int timeout) {
	int xfer;
	while (count > 0) {
		if (d->read_count >= count) {
			memcpy(buffer, d->read_ptr, count);
			d->read_count -= count;
			d->read_ptr += count;
			return 0;
		}
		if (d->read_count > 0) {
			memcpy(buffer, d->read_ptr, d->read_count);
			count -= d->read_count;
			d->read_count = 0;
		}
		xfer = usb_bulk(d->udev, d->ep_in, d->read_buffer, d->read_size, 1000);
		if (xfer < 0)
			return -1;
		if (xfer < 2)
			return -1;
		/* discard header */
		d->read_ptr = d->read_buffer + 2;
		d->read_count = xfer - 2;
	}
	return 0;
}

static int _jtag_close(JDRV *d) {
	if (d->udev) {
		//TODO: close
	}
	free(d);
	return 0;
}

static int _jtag_init(JDRV *d) {
	d->speed = 15000;
	resetstate(d);
	if (ftdi_open(d))
		goto fail;
	if (ftdi_reset(d))
		goto fail;
	if (ftdi_mpsse_enable(d))
		goto fail;
	if (usb_bulk(d->udev, d->ep_out, mpsse_init, sizeof(mpsse_init), 1000) != sizeof(mpsse_init))
		goto fail;
	return 0;
	
fail:
	_jtag_close(d);
	return -1;
}

#if TRACE_DISASM
static void pbin(u32 val, u32 bits) {
	u32 n;
	for (n = 0; n < bits; n++) {
		fprintf(stderr, "%c", (val & 1) ? '1' : '0');
		val >>= 1;
	}
}

// display mpsse command stream in a (sortof) human readable form
static void dismpsse(u8 *data, u32 n) {
	u32 x, i;
	while (n > 0) {
		fprintf(stderr,"%02x: ", data[0]);
		switch(data[0]) {
		case 0x6B: // tms rw
			fprintf(stderr, "x1 <- TDO, ");
			// fall through
		case 0x4B: // tms wo
			fprintf(stderr, "TMS <- ");
			pbin(data[2],data[1]+1);
			fprintf(stderr, ", TDI <- ");
			pbin((data[2] & 0x80) ? 0xFF : 0, data[1] + 1);
			fprintf(stderr, "\n");
			data += 3;
			n -= 3;
			break;
		case 0x2A: // ro bits
			fprintf(stderr, "x%d <- TDO\n", data[1] + 1);
			data += 2;
			n -= 2;
			break;
		case 0x28: // ro bytes
			x = ((data[2] << 8) | data[1]) + 1;
			fprintf(stderr, "x%d <- TDO\n", (int) x * 8);
			data += 3;
			n -= 3;
			break;
		case 0x1B: // wo bits
		case 0x3B: // rw bits
			fprintf(stderr, "TDI <- ");
			pbin(data[2], data[1] + 1);
			if (data[0] == 0x3B) {
				fprintf(stderr, ", x%d <- TDO\n", data[1] + 1);
			} else {
				fprintf(stderr, "\n");
			}
			data += 3;
			n -= 3;
			break;
		case 0x19: // wo bytes
		case 0x39: // rw bytes
			x = ((data[2] << 8) | data[1]) + 1;
			fprintf(stderr, "TDI <- ");
			for (i = 0; i < x; i++) pbin(data[3+i], 8);
			if (data[0] == 0x1B) {
				fprintf(stderr, ", x%d <- TDO\n", (int) x);
			} else {
				fprintf(stderr,"\n");
			}
			data += (3 + x);
			n -= (3 + x);
			break;
		case 0x87:
			fprintf(stderr,"FLUSH\n");
			data += 1;
			n -= 1;
			break;
		default:
			fprintf(stderr,"??? 0x%02x\n", data[0]);
			n = 0;
		}
	}
}
#endif

static u8 MASKBITS[9] = {
	0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF
};

static int _jtag_commit(JDRV *d) {
	unsigned n = d->next - d->cmd;
	JOP *op;
	u8 *x;

	fprintf(stderr, "jtag_commit: tx(%d) rx(%d)\n", n, (int) d->expected);
	if (d->status) {
		// if we failed during prep, error out immediately
		fprintf(stderr, "jtag_commit: pre-existing errors\n");
		goto fail;
	}
	if (n == 0) {
		goto done;
	}

	// always complete with an ioflush
	*d->next = 0x87;
	n++;
	d->nextop->op = OP_END;

#if TRACE_IO
	dump("tx", d->cmd, n);
#endif
#if TRACE_DISASM
	dismpsse(d->cmd, n);
#endif

	if (usb_bulk(d->udev, d->ep_out, d->cmd, n, 1000) != n) {
		fprintf(stderr, "jtag_commit: write failed\n");
		goto fail;
	}
	if (ftdi_read(d, d->cmd, d->expected, 1000)) {
		fprintf(stderr, "jtag_commit: read failed\n");	
		goto fail;
	}

#if TRACE_IO
	dump("rx", d->cmd, d->expected);
#endif

	for (op = d->op, x = d->cmd;;op++) {
		switch(op->op) {
		case OP_END:
			goto done;
		case OP_BITS:
			*op->ptr = ((*x) >> (8 - op->n)) & MASKBITS[op->n];
			x++;
			break;
		case OP_BYTES:
			memcpy(op->ptr, x, op->n);
			x += op->n;
			break;
		case OP_1BIT:
			if (*x & op->n) {
				*op->ptr |= op->x;
			} else {
				*op->ptr &= (~op->x);
			}
			x++;
			break;
		}
	}
done:
	resetstate(d);
	return 0;
fail:
	resetstate(d);
	return -1;
}

static int _jtag_scan_tms(JDRV *d, u32 obit,
	u32 count, u8 *tbits, u32 ioffset, u8 *ibits) {
	if ((count > 6) || (count == 0)) {
		fprintf(stderr, "jtag_scan_tms: invalid count %d\n", (int) count);
		return (d->status = -1);
	}
	if (cmd_avail(d) < 4) {
		if (_jtag_commit(d))
			return (d->status = -1);
	}
	*d->next++ = ibits ? 0x6B : 0x4B;
	*d->next++ = count - 1;
	*d->next++ = ((obit & 1) << 7) | (*tbits);
	if (ibits) {
		if (ioffset > 7) {
			ibits += (ioffset >> 3);
			ioffset &= 7;
		}
		d->nextop->op = OP_1BIT;
		d->nextop->ptr = ibits;
		d->nextop->n = 1 << (8 - count);
		d->nextop->x = 1 << ioffset;
		d->nextop++;
		d->expected++;
	}
	return -1;
}

static int _jtag_scan_io(JDRV *d, u32 count, u8 *obits, u8 *ibits) {
	u32 n;
	u32 bcount = count >> 3;
	u8 bytecmd;
	u8 bitcmd;

	// determine operating mode and ftdi mpsse command byte
	if (obits) {
		if (ibits) {
			// read-write
			bytecmd = 0x39;
			bitcmd = 0x3B;
		} else {
			// write-only
			bytecmd = 0x19;
			bitcmd = 0x1B;
		}
	} else {
		if (ibits) {
			// read-only
			bytecmd = 0x28;
			bitcmd = 0x2A;
		} else {
			// do nothing?!
			fprintf(stderr, "jtag_scan_io: no work?!\n");
			return (d->status = -1);
		}
	}

	// do as many bytemoves as possible first
	// TODO: for exactly 1 byte, bitmove command is more efficient
	while (bcount > 0) {
		n = cmd_avail(d);

fprintf(stderr,"io bcount=%d n=%d\n",bcount,n);
		if (n < 16) {
			if (_jtag_commit(d))
				return (d->status = -1);
			continue;
		}
		n -= 4; // leave room for header and io commit
		if (n > bcount)
			n = bcount;
		*d->next++ = bytecmd;
		*d->next++ = (n - 1);
		*d->next++ = (n - 1) >> 8;
		if (obits) {
			memcpy(d->next, obits, n);
			d->next += n;
			obits += n;
		}
		if (ibits) {
			d->nextop->op = OP_BYTES;
			d->nextop->ptr = ibits;
			d->nextop->n = n;
			d->nextop++;
			ibits += n;
			d->expected += n;
		}
		bcount -= n;
	}

	// do a bitmove for any leftover bits
	count = count & 7;
	if (count == 0)
       		return 0;
	if (cmd_avail(d) < 4) {
		if (_jtag_commit(d))
			return (d->status = -1);
	}
	*d->next++ = bitcmd;
	*d->next++ = count - 1;
	if (obits) {
		*d->next++ = *obits;
	}
	if (ibits) {
		d->nextop->op = OP_BITS;
		d->nextop->ptr = ibits;
		d->nextop->n = count;
		d->nextop++;
		d->expected++;
	}
	return 0;
}

static JDVT vtable = {
	.init = _jtag_init,
	.close = _jtag_close,
	.setspeed = _jtag_setspeed,
	.commit = _jtag_commit,
	.scan_tms = _jtag_scan_tms,
	.scan_io = _jtag_scan_io,
};

int jtag_mpsse_open(JTAG **jtag) {
	JDRV *d;

       	if ((d = malloc(sizeof(JDRV))) == 0) {
		return -1;
	}
	memset(d, 0, sizeof(JDRV));

	if (jtag_init(jtag, d, &vtable)) {
		_jtag_close(d);
		return -1;
	}

	return _jtag_init(d);
}

