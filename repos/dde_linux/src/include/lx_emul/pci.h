/*
 * \brief  Lx_emul support for accessing PCI devices
 * \author Stefan Kalkowski
 * \date   2022-05-23
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PCI_H_
#define _LX_EMUL__PCI_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lx_emul_add_resource_callback_t)
	(void        * dev,
	 unsigned      number,
	 unsigned long addr,
	 unsigned long size,
	 int           io_port);


typedef void (*lx_emul_add_device_callback_t)
	(void             * bus,
	 unsigned           number,
	 char const * const name,
	 unsigned short     vendor_id,
	 unsigned short     device_id,
	 unsigned short     sub_vendor,
	 unsigned short     sub_device,
	 unsigned int       class_code,
	 unsigned char      revision,
	 unsigned           irq);


void lx_emul_pci_for_each_resource(const char * const name, void * dev,
                                   lx_emul_add_resource_callback_t fn);

void lx_emul_pci_for_each_device(void * bus, lx_emul_add_device_callback_t fn);

void lx_emul_pci_enable(const char * const name);

void * lx_emul_pci_root_bus(void);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__PCI_H_ */

