/*
 * \brief  OSS environment
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <timer_session/connection.h>
#include <pci_device/client.h>

extern "C"
{
#include <oss_config.h>
#include <oss_pci.h>
#include <dde_kit/pci.h>
#include <dde_kit/pgtab.h>
#include <dde_kit/resources.h>
}

#include <audio.h>
#include <signal.h>

/******************
 ** oss_config.h **
 ******************/

int oss_num_cards = 0;


/**********
 ** os.h **
 **********/

using namespace Genode;

static Timer::Connection _timer;

void oss_udelay(unsigned long usecs)
{
	unsigned long start = GET_JIFFIES();
	/* check for IRQs etc */
	Service_handler::s()->check_signal(false);

	unsigned delta = (GET_JIFFIES() - start) * 10000;

	/* return if already expired */
	if (delta > usecs)
		return;

	usecs -= delta;
	/* delay */
	_timer.msleep(usecs < 1000 ? 1 : usecs / 1000);
}


/*********
 ** PCI **
 *********/

extern "C" int pci_read_config_byte(oss_device_t *osdev, offset_t where, unsigned char *val)
{
	dde_kit_pci_readb(osdev->bus, osdev->dev, osdev->fun, where, val);
	return *val == ~0 ? -1 : 0;
}


extern "C" int pci_write_config_byte(oss_device_t *osdev, offset_t where, unsigned char val)
{
	dde_kit_pci_writeb(osdev->bus, osdev->dev, osdev->fun, where, val);
	return 0;
}


extern "C" int pci_read_config_word(oss_device_t *osdev, offset_t where, unsigned short *val)
{
	dde_kit_pci_readw(osdev->bus, osdev->dev, osdev->fun, where, val);
	return *val == ~0 ? -1 : 0;
}


extern "C" int pci_write_config_word(oss_device_t *osdev, offset_t where, unsigned short val)
{
	dde_kit_pci_writew(osdev->bus, osdev->dev, osdev->fun, where, val);
	return 0;
}


extern "C" int pci_read_config_dword(oss_device_t *osdev, offset_t where, unsigned int *val)
{
	dde_kit_pci_readl(osdev->bus, osdev->dev, osdev->fun, where, val);
	return *val == ~0U ? -1 : 0;
}


extern "C" int pci_read_config_irq(oss_device_t *osdev, offset_t where, unsigned char *val)
{
	int ret  = pci_read_config_byte(osdev, where, val);
	return ret;
}


extern "C" void *pci_map(oss_device_t *osdev, int resource, addr_t phys, size_t size)
{
	addr_t addr;
	if (dde_kit_request_mem(phys, size, 0, &addr))
		return 0;

	return (void *)addr;
}


extern "C" oss_native_word pci_map_io(struct _oss_device_t *osdev, int resource, unsigned base)
{
	if (resource >= Pci::Device::NUM_RESOURCES || resource < 0 ||
	    !osdev->res[resource].io)
		return 0;

	dde_kit_request_io(osdev->res[resource].base, osdev->res[resource].size,
	                   resource, osdev->bus, osdev->dev, osdev->fun);

	return  base;
}


/*************
 ** PORT/IO **
 *************/

unsigned char io_inb(struct _oss_device_t *osdev, addr_t port)
{
	return osdev->drv->inb_quirk ? osdev->drv->inb_quirk(osdev, port)
	                             : dde_kit_inb(port);
}

/*********
 ** OSS **
 *********/

extern "C" int oss_register_interrupts(oss_device_t * osdev, int intrnum,
                                       oss_tophalf_handler_t top,
                                       oss_bottomhalf_handler_t bottom)
{
	if (!osdev || intrnum != 0 || !top) {
		PWRN("Bad interrupt index %d, bad device (%p), or bad handler %p",
		     intrnum, osdev, top);
		return OSS_EINVAL;
	}

	unsigned char irq;
	pci_read_config_irq(osdev, PCI_INTERRUPT_LINE, &irq);

	/* setup bottom and top half handlers */
	osdev->irq_top    = top;
	osdev->irq_bottom = bottom;

	request_irq(irq, osdev);

	return 0;
}


extern "C" int uiomove (void *address, size_t nbytes, enum uio_rw rwflag, uio_t * uio)
{
	if (rwflag != uio->rw || nbytes > uio->size)
		return -1;

	void *target = rwflag == UIO_READ ? (void *)uio->data : address;
	void *source = rwflag == UIO_READ ? address : (void *)uio->data;
	nbytes = nbytes > uio->size ? uio->size : nbytes;
	Genode::memcpy(target, source, nbytes);
	uio->size -= nbytes;
	uio->data += nbytes;

	return 0;
}


/**
 * Wait queues
 */
extern "C" int oss_sleep(struct oss_wait_queue *wq, oss_mutex_t * mutex, int ticks,
                         oss_native_word * flags, unsigned int *status)
{
	*status = 0;

	if (!wq)
		return 0;

	wq->blocked = 1;
	unsigned long start = GET_JIFFIES();

	while (wq->blocked) {
		Irq::check_irq(true);
	
		if (GET_JIFFIES() - start > (unsigned long)ticks) {
			return 0;
		}
	}

	return 1;
}


extern "C" void oss_wakeup(struct oss_wait_queue *wq, oss_mutex_t * mutex,
                           oss_native_word * flags, short events)
{
	if (wq)
		wq->blocked = 0;
}


extern "C" oss_wait_queue_t  *oss_create_wait_queue(oss_device_t * osdev, const char *name)
{
	return new (env()->heap()) oss_wait_queue;
}


/*********
 ** DMA **
 *********/

extern "C" void * dma_alloc(oss_native_word *phys, size_t size)
{
	void *virt = KERNEL_MALLOC(size);
	*phys = (oss_native_word)dde_kit_pgtab_get_physaddr(virt);

	return virt;
}


extern "C" int __oss_alloc_dmabuf (int dev, dmap_p dmap, unsigned int alloc_flags,
                                   oss_uint64_t maxaddr, int direction)
{
	/* allocate DMA buffer depending on flags */
	oss_native_word phys;
	int size = 64 * 1024;

	if (dmap->dmabuf)
		return 0;

	if (dmap->flags & DMAP_SMALLBUF)
		size = SMALL_DMABUF_SIZE;
	if (dmap->flags & DMAP_MEDIUMBUF)
		size = MEDIUM_DMABUF_SIZE;

	if ((alloc_flags & DMABUF_SIZE_16BITS) && size > 32 * 1024)
		size = 32 * 1024;
	if (alloc_flags & DMABUF_LARGE)
		size = 356 * 1024;
	
	if (!(dmap->dmabuf = (unsigned char*)dma_alloc(&phys, size)))
		return OSS_ENOMEM;

	dmap->buffsize = size;
	dmap->dmabuf_phys = phys;

	return 0;
}


void oss_free_dmabuf (int dev, dmap_p dmap)
{
	if (!dmap->dmabuf)
		return;

	KERNEL_FREE(dmap->dmabuf);
	dmap->dmabuf = 0;
}


/**************
 ** string.h **
 **************/

extern "C" {

size_t strlen(const char *s)
{
	return Genode::strlen(s);
}


char *strcpy(char *to, const char *from)
{
	char *save = to;

	for (; (*to = *from); ++from, ++to);
	return(save);
}


char *strncpy(char *to, const char *from, size_t n)
{
	return Genode::strncpy(to, from, n);
}


int strcmp(const char *s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


int sprintf(char *str, const char *format, ...)
{
	
	va_list list;
	va_start(list, format);

	String_console sc(str, 1024);

	sc.vprintf(format, list);

	va_end(list);

	return sc.len();
}
} /* extern "C" */


static oss_cdev_drv_t *dsp_drv; /* DSP device */
static oss_cdev_drv_t *mix_drv; /* mixer */


/**
 * Internal ioctl
 */
int ioctl_dsp(int cmd, ioctl_arg arg)
{
	return dsp_drv->ioctl(0, 0, cmd, arg);
}


/**
 * Setup DSP
 */
int audio_init()
{

	if (!dsp_drv) {
		PERR("No output devices");
		return -1;
	}

	/* open device */
	int new_dev = 0;
	if (dsp_drv->open(0, 0, 0, 0, 0, &new_dev)) {
		PERR("Error opening sound card");
		return -2;
	}

	/* set fragment policy (TODO: make configurable) */
	int policy = 1;
	if (ioctl_dsp(SNDCTL_DSP_POLICY, &policy) == -1)
		PERR("Error setting policy");

	/* set format */
	int val = AFMT_S16_LE;
	if (ioctl_dsp(SNDCTL_DSP_SETFMT, &val) == -1) {
		PERR("Error setting audio format to S16_LE");
		return -3;
	}

	/* set two channels */
	val = 2;
	if (ioctl_dsp(SNDCTL_DSP_CHANNELS, &val) == -1) {
		PERR("Error enabling two channels");
		return -4;
	}

	/* set sample rate */
	val = 44100;
	if (ioctl_dsp(SNDCTL_DSP_SPEED, &val) == -1) {
		PERR("Error setting sample rate to %d HZ", val);
		return -5;
	}

	return 0;
}


int audio_play(short *data, unsigned size)
{
		uio_t io = { (char *)data, size, UIO_WRITE };
		int ret;

	if ((ret = dsp_drv->write(0, 0, &io, size)) != (int)size) 
		PERR("Error writing data s: %d r: %d func %p", size, ret, dsp_drv->write);

	Irq::check_irq();

	return 0;
}


extern "C" void oss_install_chrdev (oss_device_t * osdev, char *name, int dev_class,
                                    int instance, oss_cdev_drv_t * drv, int flags)
{
	/* only look for first mixer and first dsp */
	if (instance != 0)
		return;

	if (dev_class == OSS_DEV_DSP) {
		PINF("Found dsp: '%s'", name ? name : "<noname>");
		dsp_drv = drv;
	}

	if (dev_class == OSS_DEV_MIXER) {
		PINF("Found mixer: '%s'", name ? name : "<noname>");
		mix_drv = drv;
	}
}

