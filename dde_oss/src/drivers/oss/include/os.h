/*
 * \brief  OS specific definitions
 * \author Sebastian Sumpf
 * \date   2012-10-19
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS_H_
#define _INCLUDE__OS_H_

#ifdef __cplusplus
extern "C" {
#endif


/* DDE kit includes */
#include <dde_kit/memory.h>
#include <dde_kit/printf.h>
#include <dde_kit/types.h>
#include <dde_kit/resources.h>
#include <dde_kit/timer.h>

/* OSS includes */
#include <oss_errno.h>

#include <devid.h>

#define VERBOSE_OSS 0


/*******************
 ** Configuration **
 *******************/

/* not really used */
#define OS_VERSION
#define OSS_LICENSE "BSD"
#define OSS_BUILD_ID ""
#define OSS_COMPILE_DATE __DATE__
#define NULL 0


/***********
 ** Types **
 ***********/

typedef dde_kit_uint64_t oss_uint64_t;
typedef dde_kit_int64_t  oss_int64_t;
typedef dde_kit_addr_t   oss_native_word;
typedef void *           oss_dma_handle_t;
typedef int              oss_mutex_t;
typedef int              oss_poll_event_t;

typedef dde_kit_size_t   size_t;
typedef void             dev_info_t;
typedef int              pid_t;
typedef dde_kit_addr_t   offset_t;
typedef dde_kit_addr_t   addr_t;
typedef unsigned         timeout_id_t;

typedef dde_kit_uint8_t  uint8_t;
typedef dde_kit_uint16_t uint16_t;
typedef dde_kit_int16_t  int16_t;
typedef dde_kit_uint32_t uint32_t;
typedef dde_kit_int32_t  int32_t;
typedef dde_kit_uint64_t uint64_t;
typedef dde_kit_int64_t  int64_t;


struct fileinfo
{
	int mode;
	int acc_flags;
};

#define ISSET_FILE_FLAG(fileinfo, flag)  (fileinfo->acc_flags & (flag) ? 1 : 0)

enum uio_rw { UIO_READ, UIO_WRITE };

/*
 * IO vector
 */
typedef struct uio
{
	char  *data;
	size_t size;
	enum uio_rw rw;
} uio_t;

/*
 * Copy uio vector data to address
 */
int uiomove (void *address, size_t nbytes, enum uio_rw rwflag, uio_t * uio_p);

/*
 * Debugging
 */
enum {
	CE_PANIC = 1,
	CE_WARN  = 2,
	CE_NOTE  = 3,
	CE_CONT  = 4,
};

#define cmn_err(level, format, arg...) \
{ \
		if (level < VERBOSE_OSS) \
			dde_kit_printf(format, ##arg); \
}

#define DDB(x) x


/***********
 ** Posix **
 ***********/

/**************
 ** string.h **
 **************/

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);


int    strcmp(const char *s1, const char *s2);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *);


/*************
 ** stdio.h **
 *************/

int sprintf(char *str, const char *format, ...);


/*************
 ** fcntl.h **
 *************/

enum {
	O_ACCMODE =  0x3,
	O_NONBLOCK = 0x40000,
};


/************
 ** poll.h **
 ************/

enum {
	POLLIN,
	POLLRDNORM,
	POLLWRNORM,
	POLLOUT,
};


/*********
 ** OSS **
 *********/

#define TRACE \
{ \
	if (VERBOSE_OSS) \
		dde_kit_printf("\033[32m%s\033[0m called, not implemented\n", __PRETTY_FUNCTION__); \
}

#define TRACEN(name) \
{ \
	if (VERBOSE_OSS) \
		dde_kit_printf("\033[32m%s\033[0m called, not implemented\n", name); \
}

#define HZ DDE_KIT_HZ
#define GET_JIFFIES() jiffies


#define MUTEX_INIT(osdev, mutex, hier)
#define MUTEX_ENTER_IRQDISABLE(mutex, flags) { flags = 1; }
#define MUTEX_ENTER(mutex, flags) { flags = 1; }
#define MUTEX_EXIT(mutex, flags) { flags--; }
#define MUTEX_EXIT_IRQRESTORE(mutex, flags) { flags--; }
#define MUTEX_CLEANUP(mutex)


#define GET_PROCESS_NAME(f) NULL
#define GET_PROCESS_PID(p)  -1

#define KERNEL_MALLOC(size) (dde_kit_large_malloc(size))
#define KERNEL_FREE(ptr)    (dde_kit_large_free(ptr))

void * dma_alloc(oss_native_word *phys, size_t size);
#define CONTIG_MALLOC(osdev, sz, memlimit, phaddr, handle) dma_alloc(phaddr, sz)
#define CONTIG_FREE(osdev, p, sz, handle) KERNEL_FREE(p)


/*
 * OSS device */
struct resource
{
	unsigned base;
	unsigned size;
	unsigned io;
};

struct _oss_device_t
{
	void *devc;
	int  cardnum;
	int  available;
	char *hw_info;
	char nick[32];
	char handle[32];
	/* audio */
	int  num_audio_engines;
	int  num_audiorec;
	int  num_audioduplex;

	/* mixer */
	int num_mixerdevs;

	/* midi */
	int num_mididevs;

	/* PCI/IRQ */
	int  bus, dev, fun;
	oss_tophalf_handler_t    irq_top;
	oss_bottomhalf_handler_t irq_bottom;

	struct resource res[5];

	int first_mixer; /* This must be set to -1 by osdev_create() */

	struct oss_driver *drv; /* driver for this device */
};

/*
 * I/O mem/ports
 */
void * pci_map(struct _oss_device_t *osdev, int resource, addr_t phys, size_t size);

#define MAP_PCI_MEM(osdev, ix, phaddr, size) pci_map(osdev, ix, phaddr, size)

oss_native_word pci_map_io(struct _oss_device_t *osdev, int resource, unsigned base);

#define MAP_PCI_IOADDR(osdev, nr, io) pci_map_io(osdev, nr, io)

#define UNMAP_PCI_MEM(osdev, ix, ph, virt, size) TRACEN("UNMAP_PCI_MEM")
#define UNMAP_PCI_IOADDR(osdev, ix) {}


struct oss_driver
{
	char             *name;
	device_id_t      *id_table;
	int (*attach)(struct _oss_device_t *osdev);
	int (*detach)(struct _oss_device_t *osdev);
	unsigned char (*inb_quirk)(struct _oss_device_t *osdev, addr_t port);
};

/*
 * Used for blocking, is unblocked during IRQs
 */
struct oss_wait_queue
{
	int blocked;
};

void oss_udelay(unsigned long ticks);

timeout_id_t timeout (void (*func) (void *), void *arg, unsigned long long ticks);
void untimeout(timeout_id_t id);


/*********
 ** PCI **
 *********/

#define PCI_READB(odev, addr) (*(volatile unsigned char *)(addr))
#define PCI_WRITEB(osdev, addr, value) (*(volatile unsigned char *)(addr) = (value))
#define PCI_READW(odev, addr) (*(volatile unsigned short *)(addr))
#define PCI_WRITEW(osdev, addr, value) (*(volatile unsigned short *)(addr) = (value))
#define PCI_READL(odev, addr) (*(volatile unsigned int *)(addr))
#define PCI_WRITEL(osdev, addr, value) (*(volatile unsigned int *)(addr) = (value))


/*************
 ** Port IO **
 *************/

unsigned char io_inb(struct _oss_device_t *osdev, addr_t port);

#define INB(osdev, port) io_inb(osdev, port)
#define INW(osdev, port) dde_kit_inw(port)
#define INL(osdev, port) dde_kit_inl(port)

#define OUTB(osdev, val, port)  dde_kit_outb(port, val)
#define OUTW(osdev, val, port)  dde_kit_outw(port, val)
#define OUTL(osdev, val, port)  dde_kit_outl(port, val)


/**********************
 ** Genode interface **
 **********************/

void register_driver(struct oss_driver *driver);
void probe_drivers(void);
void request_irq(unsigned irq, struct _oss_device_t *osdev);


#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__OS_H_ */
