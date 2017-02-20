/*
 * \brief  Emulation of the OpenBSD kernel API
 * \author Josef Soentgen
 * \date   2014-11-09
 *
 * The content of this file, in particular data structures, is partially
 * derived from OpenBSD-internal headers.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BSD_EMUL_H_
#define _BSD_EMUL_H_

#include <extern_c_begin.h>


/*
 * These emul specific functions are called by the OpenBSD
 * audio framework if an play/record interrupt has occured
 * (see patches/notify.patch).
 */

void notify_play();
void notify_record();


/*****************
 ** sys/types.h **
 *****************/

typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;

typedef unsigned char   u_int8_t;
typedef unsigned short  u_int16_t;
typedef unsigned int    u_int32_t;

typedef unsigned int   uint;

typedef signed short       int16_t;
typedef signed   int       int32_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef __SIZE_TYPE__      size_t;
typedef signed long        ssize_t;

typedef char *        caddr_t;
typedef unsigned long paddr_t;

typedef signed int       dev_t;
typedef signed long long off_t;

#define     minor(x)        ((int32_t)((x) & 0xff) | (((x) & 0xffff0000) >> 8))


/*****************
 ** sys/errno.h **
 *****************/

enum {
	EIO         = 5,
	ENXIO       = 6,
	ENOMEM      = 12,
	EACCES      = 13,
	EBUSY       = 16,
	ENODEV      = 19,
	EINVAL      = 22,
	ENOTTY      = 25,
	EAGAIN      = 35,
	EWOULDBLOCK = EAGAIN,
	ETIMEDOUT   = 60,
};


/******************
 ** sys/signal.h **
 ******************/

enum {
	SIGIO = 23,
};


/******************
 ** sys/malloc.h **
 ******************/

enum {
	/* malloc flags */
	M_WAITOK = 0x01,
	M_NOWAIT = 0x02,
	M_ZERO   = 0x08,
	/* types of memory */
	M_DEVBUF = 2,
};

void *malloc(size_t, int, int);
void *mallocarray(size_t, size_t, int, int);
void free(void *, int, size_t);


/*****************
 ** sys/param.h **
 *****************/

enum {
	PZERO = 22,
	PWAIT = 32,

	PCATCH = 0x100,
};

#ifdef __cplusplus
#define NULL 0
#else
#define NULL (void*)0
#endif /* __cplusplus */

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))


/******************
 ** sys/kernel.h **
 ******************/

enum { HZ = 100 };


extern int hz;


/*****************
 ** sys/cdefs.h **
 *****************/

#define __packed __attribute((__packed__))


/****************
 ** sys/proc.h **
 ****************/

struct proc { };


/****************
 ** sys/task.h **
 ****************/

struct task { };

/***************
 ** sys/uio.h **
 ***************/

enum uio_rw
{
	UIO_READ  = 0,
	UIO_WRITE = 1,
};

struct uio
{
	off_t  uio_offset;
	size_t uio_resid;
	enum   uio_rw uio_rw;

	/* emul specific fields */
	void *buf;
	size_t buflen;
};


/*****************
 ** sys/event.h **
 *****************/

enum {
	EVFILT_READ  = -1,
	EVFILT_WRITE = -2,
};

struct kevent
{
	short filter;
};

#include <sys/queue.h>

struct knote;
SLIST_HEAD(klist, knote);

struct filterops
{
	int  f_isfd;
	int  (*f_attach)(struct knote*);
	void (*f_detach)(struct knote*);
	int  (*f_event)(struct knote*, long);
};

struct knote
{
	SLIST_ENTRY(knote) kn_selnext;
	struct kevent           kn_kevent;
#define kn_filter kn_kevent.filter

	const struct filterops *kn_fop;
	void                   *kn_hook;
};


/*******************
 ** sys/selinfo.h **
 *******************/

struct selinfo
{
	struct klist si_note;
};

void selrecord(struct proc *selector, struct selinfo *);
void selwakeup(struct selinfo *);


/*******************
 ** machine/cpu.h **
 *******************/

struct cpu_info { };
extern struct cpu_info cpu_info_primary;

#define curcpu() (&cpu_info_primary)


/*********************
 ** machine/mutex.h **
 *********************/

#define MUTEX_INITIALIZER(ipl) { 0, (ipl), 0, NULL }
#define MUTEX_ASSERT_UNLOCK(mtx) do {                        \
	if ((mtx)->mtx_owner != curcpu())                        \
		panic("mutex %p not held in %s\n", (mtx), __func__); \
} while (0)
#define MUTEX_ASSERT_LOCKED(mtx) do {                     \
	if ((mtx)->mtx_owner != curcpu())                     \
	panic("mutex %p not held in %s\n", (mtx), __func__); \
} while (0)
#define MUTEX_ASSERT_UNLOCKED(mtx) do {              \
	if ((mtx)->mtx_owner == curcpu())                \
	panic("mutex %p held in %s\n", (mtx), __func__); \
} while (0)

struct mutex
{
	volatile int  mtx_lock;
	int           mtx_wantipl; /* interrupt priority level */
	int           mtx_oldipl;
	void         *mtx_owner;
};


/*****************
 ** sys/mutex.h **
 *****************/

void mtx_enter(struct mutex *);
void mtx_leave(struct mutex *);


/*****************
 ** sys/systm.h **
 *****************/

extern int nchrdev;

int enodev(void);

int printf(const char *, ...);
int snprintf(char *buf, size_t, const char *, ...);
void panic(const char *, ...);

void bcopy(const void *, void *, size_t);
void bzero(void *, size_t);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);

void wakeup(const volatile void*);
int tsleep(const volatile void *, int, const char *, int);
int msleep(const volatile void *, struct mutex *, int,  const char*, int);

int uiomove(void *, int, struct uio *);


/*******************
 ** lib/libkern.h **
 *******************/

static inline u_int max(u_int a, u_int b) { return (a > b ? a : b); }
static inline u_int min(u_int a, u_int b) { return (a < b ? a : b); }

size_t strlcpy(char *, const char *, size_t);
int strcmp(const char *, const char *);


/*********************
 ** machine/param.h **
 *********************/

#define DELAY(x) delay(x)
void delay(int);


/*******************************
 ** machine/intrdefs.h **
 *******************************/

enum {
	IPL_AUDIO  = 8,
	IPL_MPSAFE = 0x100,
};


/*****************
 ** sys/fcntl.h **
 *****************/

enum {
	FREAD  = 0x0001,
	FWRITE = 0x0002,
};


/****************
 ** sys/poll.h **
 ****************/

enum {
	POLLIN     = 0x0001,
	POLLOUT    = 0x0004,
	POLLERR    = 0x0008,
	POLLRDNORM = 0x0040,
	POLLWRNORM = POLLOUT,
};


/*****************
 ** sys/vnode.h **
 *****************/

enum vtype {
	VCHR,
};

enum {
	IO_NDELAY = 0x10,
};

void vdevgone(int, int, int, enum vtype);


/******************
 ** sys/ioccom.h **
 ******************/

#define IOCPARM_MASK    0x1fff
#define IOCPARM_LEN(x)  (((x) >> 16) & IOCPARM_MASK)
#define IOCGROUP(x) (((x) >> 8) & 0xff)

#define IOC_VOID    (unsigned long)0x20000000
#define IOC_OUT     (unsigned long)0x40000000
#define IOC_IN      (unsigned long)0x80000000
#define IOC_INOUT   (IOC_IN|IOC_OUT)

#define _IOC(inout,group,num,len) \
	(inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define _IO(g,n)    _IOC(IOC_VOID,  (g), (n), 0)
#define _IOR(g,n,t) _IOC(IOC_OUT,   (g), (n), sizeof(t))
#define _IOW(g,n,t) _IOC(IOC_IN,    (g), (n), sizeof(t))
#define _IOWR(g,n,t)    _IOC(IOC_INOUT, (g), (n), sizeof(t))


/*****************
 ** sys/filio.h **
 *****************/

#define FIONBIO  _IOW('f', 126, int)
#define FIOASYNC _IOW('f', 125, int)


/***************
 ** sys/tty.h **
 ***************/

struct tty { };


/****************
 ** sys/conf.h **
 ****************/

struct cdevsw
{
	int         (*d_open)(dev_t dev, int oflags, int devtype, struct proc *p);
	int         (*d_close)(dev_t dev, int fflag, int devtype, struct proc *);
	int         (*d_read)(dev_t dev, struct uio *uio, int ioflag);
	int         (*d_write)(dev_t dev, struct uio *uio, int ioflag);
	int         (*d_ioctl)(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p);
	int         (*d_stop)(struct tty *tp, int rw);
	struct tty *(*d_tty)(dev_t dev);
	int         (*d_poll)(dev_t dev, int events, struct proc *p);
	paddr_t     (*d_mmap)(dev_t, off_t, int);
	u_int       d_type;
	u_int       d_flags;
	int         (*d_kqfilter)(dev_t dev, struct knote *kn);
};

extern struct cdevsw cdevsw[];

/**
 * Normally these functions are defined by macro magic but we declare
 * them verbatim.
 */
int audioopen(dev_t, int, int, struct proc *);
int audioclose(dev_t, int, int, struct proc *);
int audioread(dev_t, struct uio *, int);
int audiowrite(dev_t, struct uio *, int);
int audioioctl(dev_t, u_long, caddr_t, int, struct proc *);
int audiostop(struct tty *, int);
struct tty *audiotty(dev_t);
int audiopoll(dev_t, int, struct proc *);
paddr_t audiommap(dev_t, off_t, int);
int audiokqfilter(dev_t, struct knote *);

#define NMIDI 0

/******************
 ** sys/select.h **
 ******************/

enum {
	NBBY = 8, /* num of bits in byte */
};


/*********************
 ** sys/singalvar.h **
 *********************/

void psignal(struct proc *p, int sig);


/******************
 ** sys/rndvar.h **
 ******************/

#define add_audio_randomness(d)


/*******************
 ** machine/bus.h **
 *******************/

typedef u_long bus_addr_t;
typedef u_long bus_size_t;
typedef u_long bus_space_handle_t;

struct bus_dma_segment
{
	bus_addr_t  ds_addr;

	/* emul specific fields */
	bus_size_t  ds_size;
};
typedef struct bus_dma_segment bus_dma_segment_t;

typedef struct bus_dmamap* bus_dmamap_t;
struct bus_dmamap
{
	bus_dma_segment_t dm_segs[1];

	/* emul specific fields */
	bus_size_t size;
	bus_size_t maxsegsz;
	int nsegments;
};

typedef struct bus_dma_tag             *bus_dma_tag_t;
struct bus_dma_tag
{
	void    *_cookie; /* cookie used in the guts */

	/*
	 * DMA mapping methods.
	 */
	int  (*_dmamap_create)(bus_dma_tag_t, bus_size_t, int,
	                      bus_size_t, bus_size_t, int, bus_dmamap_t *);
	void (*_dmamap_destroy)(bus_dma_tag_t, bus_dmamap_t);
	int  (*_dmamap_load)(bus_dma_tag_t, bus_dmamap_t, void *,
	                     bus_size_t, struct proc *, int);
	void (*_dmamap_unload)(bus_dma_tag_t, bus_dmamap_t);

	/*
	 * DMA memory utility functions.
	 */
	int     (*_dmamem_alloc)(bus_dma_tag_t, bus_size_t, bus_size_t,
	                         bus_size_t, bus_dma_segment_t *, int, int *, int);
	void    (*_dmamem_free)(bus_dma_tag_t, bus_dma_segment_t *, int);
	int     (*_dmamem_map)(bus_dma_tag_t, bus_dma_segment_t *,
	                       int, size_t, caddr_t *, int);
	void    (*_dmamem_unmap)(bus_dma_tag_t, caddr_t, size_t);
	paddr_t (*_dmamem_mmap)(bus_dma_tag_t, bus_dma_segment_t *,
	                         int, off_t, int, int);

};

int     bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t, bus_size_t, int, bus_dmamap_t *);
void    bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int     bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t, struct proc *, int);
void    bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);

int     bus_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t, bus_dma_segment_t *, int, int *, int);
void    bus_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int     bus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int, size_t, caddr_t *, int);
void    bus_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t);
paddr_t bus_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int, off_t, int, int);

typedef u_long bus_space_tag_t;

void bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

u_int8_t  bus_space_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t bus_space_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t bus_space_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void bus_space_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int8_t);
void bus_space_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int16_t);
void bus_space_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);

enum {
	BUS_DMA_WAITOK   = 0x0000,
	BUS_DMA_NOWAIT   = 0x0001,
	BUS_DMA_COHERENT = 0x0004,
};


/**********************
 ** dev/pci/pcireg.h **
 **********************/

typedef u_int16_t pci_vendor_id_t;
typedef u_int16_t pci_product_id_t;

#define PCI_VENDOR_SHIFT            0
#define PCI_VENDOR_MASK             0xffff
#define PCI_VENDOR(id) \
	(((id) >> PCI_VENDOR_SHIFT) & PCI_VENDOR_MASK)

#define PCI_PRODUCT_SHIFT           16
#define PCI_PRODUCT_MASK            0xffff
#define PCI_PRODUCT(id) \
	(((id) >> PCI_PRODUCT_SHIFT) & PCI_PRODUCT_MASK)

#define PCI_CLASS_SHIFT             24
#define PCI_CLASS_MASK              0xff
#define PCI_CLASS(cr) \
	(((cr) >> PCI_CLASS_SHIFT) & PCI_CLASS_MASK)

#define PCI_SUBCLASS_SHIFT          16
#define PCI_SUBCLASS_MASK           0xff
#define PCI_SUBCLASS(cr) \
	(((cr) >> PCI_SUBCLASS_SHIFT) & PCI_SUBCLASS_MASK)

#define PCI_REVISION_SHIFT          0
#define PCI_REVISION_MASK           0xff
#define PCI_REVISION(cr) \
	(((cr) >> PCI_REVISION_SHIFT) & PCI_REVISION_MASK)

enum {
	PCI_COMMAND_IO_ENABLE           = 0x00000001,
	PCI_COMMAND_STATUS_REG          = 0x04,
	PCI_COMMAND_BACKTOBACK_ENABLE   = 0x00000200,

	PCI_CLASS_MULTIMEDIA            = 0x04,

	PCI_SUBCLASS_MULTIMEDIA_HDAUDIO = 0x03,

	PCI_MAPREG_TYPE_MASK            = 0x00000001,
	PCI_MAPREG_MEM_TYPE_MASK        = 0x00000006,

	PCI_MAPREG_TYPE_IO              = 0x00000001,
	PCI_MAPREG_TYPE_MEM             = 0x00000000,

	PCI_SUBSYS_ID_REG               = 0x2c,

	PCI_PMCSR_STATE_D0              = 0x0000,
};

#define PCI_MAPREG_IO_ADDR(mr)                      \
	((mr) & PCI_MAPREG_IO_ADDR_MASK)
#define PCI_MAPREG_IO_SIZE(mr)                      \
	(PCI_MAPREG_IO_ADDR(mr) & -PCI_MAPREG_IO_ADDR(mr))
#define PCI_MAPREG_IO_ADDR_MASK     0xfffffffe


/**********************
 ** dev/pci/pcivar.h **
 **********************/

/* actually from pci_machdep.h */
typedef void *pci_chipset_tag_t;
typedef uint32_t pcitag_t;


typedef uint32_t pcireg_t;
struct pci_attach_args
{
	bus_dma_tag_t     pa_dmat;
	pci_chipset_tag_t pa_pc;
	pcitag_t          pa_tag;
	pcireg_t          pa_id;
	pcireg_t          pa_class;
};

struct pci_matchid {
	pci_vendor_id_t     pm_vid;
	pci_product_id_t    pm_pid;
};


int pci_matchbyid(struct pci_attach_args *, const struct pci_matchid *, int);
int pci_set_powerstate(pci_chipset_tag_t, pcitag_t, int);
int pci_mapreg_map(struct pci_attach_args *, int, pcireg_t, int,
                   bus_space_tag_t *, bus_space_handle_t *, bus_addr_t *,
                   bus_size_t *, bus_size_t);
const char *pci_findvendor(pcireg_t);

/***************************
 ** machine/pci_machdep.h **
 ***************************/

struct pci_intr_handle { unsigned irq; };
typedef struct pci_intr_handle pci_intr_handle_t;

int pci_intr_map_msi(struct pci_attach_args *, pci_intr_handle_t *);
int pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
void *pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t,
                         int, int (*)(void *), void *, const char *);
void pci_intr_disestablish(pci_chipset_tag_t, void *);

pcireg_t pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void pci_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);


/*******************
 ** sys/timeout.h **
 *******************/

struct timeout { };

void timeout_set(struct timeout *, void (*)(void *), void *);
int timeout_add_msec(struct timeout *, int);
int timeout_del(struct timeout *);


/******************
 ** sys/endian.h **
 ******************/

#define htole32(x) ((uint32_t)(x))


#include <extern_c_end.h>

#endif /* _BSD_EMUL_H_ */
