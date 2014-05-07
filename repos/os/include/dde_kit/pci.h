/*
 * \brief  PCI bus access
 * \author Christian Helmuth
 * \date   2008-10-01
 *
 * The DDE Kit provides virtual PCI bus hierarchy, which may be a subset of
 * the PCI bus with the same bus-device-function IDs.
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__PCI_H_
#define _INCLUDE__DDE_KIT__PCI_H_

#include <dde_kit/types.h>

/********************************
 ** Configuration space access **
 ********************************/

/**
 * Read byte from PCI config space
 *
 * \param bus   bus number
 * \param dev   device number
 * \param fun   function number
 * \param pos   offset in config space
 *
 * \retval val  read value
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_readb(int bus, int dev, int fun, int pos, dde_kit_uint8_t  *val);

/**
 * Read word from PCI config space
 *
 * \param bus   bus number
 * \param dev   device number
 * \param fun   function number
 * \param pos   offset in config space
 *
 * \retval val  read value
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_readw(int bus, int dev, int fun, int pos, dde_kit_uint16_t *val);

/**
 * Read dword from PCI config space
 *
 * \param bus   bus number
 * \param dev   device number
 * \param fun   function number
 * \param pos   offset in config space
 *
 * \retval val  read value
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_readl(int bus, int dev, int fun, int pos, dde_kit_uint32_t *val);

/**
 * Write byte to PCI config space
 *
 * \param bus   bus number
 * \param dev  device number
 * \param fun  function  number
 * \param pos  offset in config space
 * \param val  value to write
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_writeb(int bus, int dev, int fun, int pos, dde_kit_uint8_t val);

/**
 * Write word to PCI config space
 *
 * \param bus   bus number
 * \param dev  device number
 * \param fun  function  number
 * \param pos  offset in config space
 * \param val  value to write
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_writew(int bus, int dev, int fun, int pos, dde_kit_uint16_t  val);

/**
 * Write dword to PCI config space.
 *
 * \param bus   bus number
 * \param dev  device number
 * \param fun  function  number
 * \param pos  offset in config space
 * \param val  value to write
 *
 * \return 0 on success, -1 otherwise
 */
void dde_kit_pci_writel(int bus, int dev, int fun, int pos, dde_kit_uint32_t  val);


/***************************
 ** Convenience functions **
 ***************************/

/**
 * Find first PCI device on virtual bus tree
 *
 * \retval bus  bus number
 * \retval dev  device number
 * \retval fun  function number
 *
 * \return 0 on success, -1 otherwise
 */
int dde_kit_pci_first_device(int *bus, int *dev, int *fun);

/**
 * Find next PCI device
 *
 * \param bus   bus number to start at
 * \param dev   device number to start at
 * \param fun   function number to start at
 *
 * \retval bus  bus number
 * \retval dev  device number
 * \retval fun  function number
 *
 * \return 0 on success, -1 otherwise
 */
int dde_kit_pci_next_device(int *bus, int *dev, int *fun);

/**
 * Allocate a DMA buffer and map it. If an IOMMU is available this functions
 * takes care that DMA to this buffer for the given PCI device is permitted.
 *
 * \retval bus  bus number
 * \retval dev  device number
 * \retval fun  function number
 * 
 * \return 0 in case of failure, otherwise the virtual address of the buffer.
 */
dde_kit_addr_t dde_kit_pci_alloc_dma_buffer(int bus, int dev, int fun,
                                            dde_kit_size_t size);

/**
 * Initialize PCI subsystem
 *
 * The PCI subsystem can be instructed to request solely a specific PCI device
 * or a specific PCI subset (one class or multiple). The parameters are
 * described by the parameters device_class and class_mask, which are used to
 * filter PCI class codes as described by the pseudo code:
 *
 * for each 'pci_device' out of 'all_pci_devices' try
 * {
 *    bool nohit = (pci_device.class_code() ^ device_class) & class_mask
 *    if (!nohit)
 *      use 'pci_device' with this PCI subsystem
 * }
 *
 * If no restriction to the PCI subsystem should be applied, use 0 for the
 * device_class and class_mask.
 *
 * \param device_class filter applied with 'bitwise XOR' operand to the class
 *                     code of each PCI device
 * \param class_mask   filter applied with 'bitwise AND' operand to the result
 *                     out of device_class and PCI class code of each device
 */
void dde_kit_pci_init(unsigned device_class, unsigned class_mask);


#endif /* _INCLUDE__DDE_KIT__PCI_H_ */
