/*
 * \brief  PCI bus access
 * \author Christian Helmuth
 * \date   2008-10-01
 *
 * The DDE Kit provides virtual PCI bus hierarchy, which may be a subset of
 * the PCI bus with the same bus-device-function IDs.
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
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
 * Initialize PCI subsystem
 */
void dde_kit_pci_init(void);


#endif /* _INCLUDE__DDE_KIT__PCI_H_ */
