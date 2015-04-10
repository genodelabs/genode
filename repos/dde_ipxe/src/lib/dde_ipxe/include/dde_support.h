/*
 * \brief  DDE iPXE wrappers to C++ backend
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2013-01-07
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DDE_SUPPORT_H_
#define _DDE_SUPPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long  dde_addr_t;
typedef unsigned long  dde_size_t;
typedef unsigned char  dde_uint8_t;
typedef unsigned short dde_uint16_t;
typedef unsigned int   dde_uint32_t;


/*****************
 ** Initialization
 *****************/
void dde_init(void *ep);


/***********
 ** Timer **
 ***********/

void dde_udelay(unsigned long usecs);
void dde_mdelay(unsigned long msecs);


/************
 ** printf **
 ************/

typedef __builtin_va_list va_list;
void dde_vprintf(const char *fmt, va_list va) __attribute__ ((format (printf, 1, 0)));
void dde_printf(const char *fmt, ...);


/***************************************************
 ** Support for aligned and DMA memory allocation **
 ***************************************************/

int dde_dma_mem_init();

void *dde_dma_alloc(dde_size_t size, dde_size_t align, dde_size_t offset);
void  dde_dma_free(void *p, dde_size_t size);

dde_addr_t dde_dma_get_physaddr(void *virt);


/***************************
 ** locking/synchronizing **
 ***************************/

void dde_lock_enter(void);
void dde_lock_leave(void);


/************************
 ** Interrupt handling **
 ************************/

int dde_interrupt_attach(void (*handler)(void *), void *priv);


/******************
 ** PCI handling **
 ******************/

int dde_pci_first_device(int *, int *, int *);
int dde_pci_next_device(int *, int *, int *);

void dde_pci_readb(int, dde_uint8_t *);
void dde_pci_readw(int, dde_uint16_t *);
void dde_pci_readl(int, dde_uint32_t *);
void dde_pci_writeb(int, dde_uint8_t);
void dde_pci_writew(int, dde_uint16_t);
void dde_pci_writel(int, dde_uint32_t);


/**************
 ** I/O port **
 **************/

void dde_request_io(dde_addr_t, dde_size_t);

dde_uint8_t  dde_inb(dde_addr_t);
dde_uint16_t dde_inw(dde_addr_t);
dde_uint32_t dde_inl(dde_addr_t);

void dde_outb(dde_addr_t, dde_uint8_t);
void dde_outw(dde_addr_t, dde_uint16_t);
void dde_outl(dde_addr_t, dde_uint32_t);


/**********************
 ** Slab memory pool **
 **********************/

void *dde_slab_alloc(dde_size_t);
void  dde_slab_free(void *);


/****************
 ** I/O memory **
 ****************/

int dde_request_iomem(dde_addr_t, dde_size_t, int, dde_addr_t *);
int dde_release_iomem(dde_addr_t, dde_size_t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _DDE_SUPPORT_H_ */
