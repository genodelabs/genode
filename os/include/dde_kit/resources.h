/*
 * \brief  Hardware-resource access
 * \author Christian Helmuth
 * \date   2008-10-21
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__RESOURCES_H_
#define _INCLUDE__DDE_KIT__RESOURCES_H_

#include <dde_kit/types.h>

/**
 * Allocate I/O port range (x86)
 *
 * \param start  first port of range
 * \param size   size of port range
 *
 * \return 0 on success, -1 otherwise
 */
int dde_kit_request_io(dde_kit_addr_t start, dde_kit_size_t size);

/**
 * Free I/O port range (x86)
 *
 * \param start  first port of range
 * \param size   size of port range
 *
 * \return 0 on success, -1 otherwise
 */
int dde_kit_release_io(dde_kit_addr_t start, dde_kit_size_t size);

/**
 * Read I/O port (byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned char dde_kit_inb(dde_kit_addr_t port);

/**
 * Read I/O port (2-byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned short dde_kit_inw(dde_kit_addr_t port);

/**
 * Read I/O port (4-byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned long dde_kit_inl(dde_kit_addr_t port);

/**
 * Write I/O port (byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void dde_kit_outb(dde_kit_addr_t port, unsigned char val);

/**
 * Write I/O port (2-byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void dde_kit_outw(dde_kit_addr_t port, unsigned short val);

/**
 * Write I/O port (4-byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void dde_kit_outl(dde_kit_addr_t port, unsigned long val);

/**
 * Allocate MMIO range
 *
 * \param  start  begin of MMIO range
 * \param  size   size of MMIO range
 * \param  wc     if !0 request write-combined memory mapping
 *
 * \retval vaddr  virtual start address mapped range
 * \return 0 on success, -1 otherwise
 */
int dde_kit_request_mem(dde_kit_addr_t start, dde_kit_size_t size,
                        int wc, dde_kit_addr_t *vaddr);

/**
 * Free MMIO range
 *
 * \param  start  begin of MMIO range
 * \param  size   size of MMIO range
 *
 * \return 0 on success, -1 otherwise
 */
int dde_kit_release_mem(dde_kit_addr_t start, dde_kit_size_t size);

#endif /* _INCLUDE__DDE_KIT__RESOURCES_H_ */
