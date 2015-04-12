/*
 * \brief  DDE iPXE emulation implementation
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* iPXE */
#include <stdlib.h>
#include <ipxe/io.h>
#include <ipxe/uaccess.h>
#include <ipxe/malloc.h>
#include <ipxe/pci.h>
#include <ipxe/settings.h>
#include <ipxe/netdevice.h>
#include <ipxe/timer2.h>
/* local includes */
#include <dde_support.h>
#include "local.h"

#define PDBG(fmt, ...) dde_printf(fmt "\n", ##__VA_ARGS__)


/***************************
 ** DMA memory allocation **
 ***************************/

void *alloc_memblock(size_t size, size_t align, size_t offset) {
	return dde_dma_alloc(size, align, offset); }


void free_memblock(void *p, size_t size) {
	dde_dma_free(p, size); }


/**********************
 ** Slab memory pool **
 **********************/

static inline void *alloc_from_slab(size_t size) {
	return dde_slab_alloc(size); }


static inline void free_in_slab(void *p) {
	dde_slab_free(p); }


/************
 ** stdlib **
 ************/

void *zalloc(size_t size)
{
	char *buf = alloc_from_slab(size);

	if (buf)
		memset(buf, 0, size);

	return buf;
}


void *malloc(size_t size) {
	return alloc_from_slab(size); }


void free(void *p) {
	free_in_slab(p); }


/*********************
 ** Time and Timers **
 *********************/

void timer2_udelay(unsigned long usecs) {
	dde_udelay(usecs); }


void udelay(unsigned long usecs)
{
	static int init = 0;

	extern void __rdtsc_udelay(unsigned long usecs);

	/*
	 * On first udelay, the rdtsc implementation is calibrated. Therefore, we
	 * force a delay of 10ms to get sane values.
	 */
	if (!init) {
		__rdtsc_udelay(10000);
		init = 1;
	} else {
		__rdtsc_udelay(usecs);
	}
}


void mdelay(unsigned long msecs) {
	dde_mdelay(msecs); }


int printf(const char *format, ...)
{
	/* replace unsupported '%#' with 'x%' in format string */
	char *new_format = (char *)malloc(strlen(format) + 1);
	if (!new_format)
		return -1;
	memcpy(new_format, format, strlen(format) + 1);
	{
		int off;
		char *f;
		for (off = 0, f = new_format; *f; off++, f++)
			if (f[0] == '%' && f[1] == '#') {
				f[0] = 'x';
				f[1] = '%';
			}
	}

	va_list va;

	va_start(va, format);
	dde_vprintf(new_format, va);
	va_end(va);

	free(new_format);
	return 0;
}


/***********************************
 ** RAM and I/O memory management **
 ***********************************/

void iounmap(volatile const void *io_addr)
{
	LOG("io_addr = %p", io_addr);
	dde_release_iomem((dde_addr_t) io_addr, 1);
}


void *ioremap(unsigned long bus_addr, size_t len)
{
	LOG("bus_addr = %p len = %zx", (void *)bus_addr, len);
	dde_addr_t vaddr;

	int ret = dde_request_iomem(bus_addr, &vaddr);

	return ret ? 0 : (void *)vaddr;
}


unsigned long user_to_phys(userptr_t userptr, off_t offset)
{
	return dde_dma_get_physaddr((void *)userptr) + offset;
}


userptr_t virt_to_user(volatile const void *addr)
{
	return trivial_virt_to_user(addr);
}


unsigned long phys_to_bus(unsigned long phys_addr)
{
	return phys_addr;
}


/*******************
 ** PCI subsystem **
 *******************/

int pci_read_config_byte(struct pci_device *pci, unsigned int where, uint8_t *value)
{
	dde_pci_readb(where, value);
	return 0;
}


int pci_read_config_word(struct pci_device *pci, unsigned int where, uint16_t *value)
{
	dde_pci_readw( where, value);
	return 0;
}


int pci_read_config_dword(struct pci_device *pci, unsigned int where, uint32_t *value)
{
	dde_pci_readl(where, value);
	return 0;
}


int pci_write_config_byte(struct pci_device *pci, unsigned int where, uint8_t value)
{
	dde_pci_writeb(where, value);
	return 0;
}


int pci_write_config_word(struct pci_device *pci, unsigned int where, uint16_t value)
{
	dde_pci_writew(where, value);
	return 0;
}


int pci_write_config_dword(struct pci_device *pci, unsigned int where, uint32_t value)
{
	dde_pci_writel( where, value);
	return 0;
}


unsigned long pci_bar_start(struct pci_device *pci, unsigned int reg)
{
	/*
	 * XXX We do not check for 64-bit BARs here.
	 */

	uint32_t val;
	pci_read_config_dword(pci, reg, &val);

	if ((val & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY)
		return val & PCI_BASE_ADDRESS_MEM_MASK;
	else
		return val & PCI_BASE_ADDRESS_IO_MASK;
}


/* drivers/bus/pci.c */

void adjust_pci_device ( struct pci_device *pci ) {
	unsigned short new_command, pci_command = 0;

	pci_read_config_word(pci, PCI_COMMAND, &pci_command);
	new_command = pci_command | PCI_COMMAND_MASTER | PCI_COMMAND_MEM | PCI_COMMAND_IO;
	if (pci_command != new_command) {
		LOG("PCI BIOS has not enabled device " FMT_BUSDEVFN "! "
		    "Updating PCI command %04x->%04x\n", PCI_BUS(pci->busdevfn),
		    PCI_SLOT(pci->busdevfn), PCI_FUNC (pci->busdevfn),
		    pci_command, new_command);
		pci_write_config_word(pci, PCI_COMMAND, new_command);
	}

	unsigned char pci_latency;
	pci_read_config_byte ( pci, PCI_LATENCY_TIMER, &pci_latency);
	if ( pci_latency < 32 ) {
		LOG("PCI device " FMT_BUSDEVFN " latency timer is unreasonably "
		    "low at %d. Setting to 32.\n", PCI_BUS(pci->busdevfn),
		    PCI_SLOT ( pci->busdevfn ), PCI_FUNC ( pci->busdevfn ),
		    pci_latency );
		pci_write_config_byte ( pci, PCI_LATENCY_TIMER, 32);
	}
}


/***********************
 ** Device management **
 ***********************/

struct settings_operations generic_settings_operations = {
        .store = 0,
        .fetch = 0,
        .clear = 0,
};


int register_settings(struct settings *settings, struct settings *parent,
                      const char *name)
{
	return 0;
}


void unregister_settings(struct settings *settings) { }


void ref_increment(struct refcnt *refcnt) { }


void ref_decrement(struct refcnt *refcnt) { }
