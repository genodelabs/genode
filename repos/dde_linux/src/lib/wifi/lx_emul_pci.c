/*
 * \brief  Linux emulation environment specific to this driver (PCI)
 * \author Josef Soentgen
 * \date   2023-02-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/slab.h>

#include <lx_emul/random.h>
#include <lx_emul/alloc.h>
#include <lx_emul/io_mem.h>

#include <linux/pci.h>

int pcim_iomap_regions_request_all(struct pci_dev * pdev,int mask,const char * name)
{
	return 0;
}


#include <linux/pci.h>

static unsigned long *_pci_iomap_table;

void __iomem * const * pcim_iomap_table(struct pci_dev * pdev)
{
	unsigned i;

	if (!_pci_iomap_table)
		_pci_iomap_table = kzalloc(sizeof (unsigned long*) * 6, GFP_KERNEL);

	if (!_pci_iomap_table)
		return NULL;

	for (i = 0; i < 6; i++) {
		struct resource *r = &pdev->resource[i];
		unsigned long phys_addr = r->start;
		unsigned long size      = r->end - r->start;

		if (!phys_addr || !size)
			continue;

		_pci_iomap_table[i] =
			(unsigned long)lx_emul_io_mem_map(phys_addr, size);
	}

	return (void const *)_pci_iomap_table;
}


#include <linux/pci.h>
#include <uapi/linux/pci_regs.h>

int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	enum { PCI_CFG_RETRY_TIMEOUT = 0x41 };

	switch (where) {
		/*
		 * iwlwifi: "We disable the RETRY_TIMEOUT register (0x41) to keep
		 *           PCI Tx retries from interfering with C3 CPU state"
		 */
		case PCI_CFG_RETRY_TIMEOUT:
			return 0;

	/*
	 * rtlwifi: "leave D3 mode"
	 */
	case 0x44:
	case PCI_COMMAND:
		return 0;
	/*
	 * rtlwifi: needed for enabling DMA 64bit support
	 */
	case 0x719:
		return 0;
	/*
	 * rtlwifi: below are registers related to ASPM and PCI link
	 *          control that we do not handle (yet).
	 */
	case 0x81:
	case 0x98:
		return 0;
	};

	return -1;
}


int pci_write_config_dword(const struct pci_dev * dev,int where,u32 val)
{
	switch (where) {
	/*
	 * ath9k: "Disable the bETRY_TIMEOUT register (0x41) to keep
	 *        PCI Tx retries from interfering with C3 CPU state."
	 */
	case 0x40:
		return 0;
	}

	return -1;
}


int pci_read_config_dword(const struct pci_dev * dev,int where,u32 * val)
{
	switch (where) {
	/*
	 * ath9k: "Disable the bETRY_TIMEOUT register (0x41) to keep
	 *        PCI Tx retries from interfering with C3 CPU state."
	 */
	case 0x40:
		return 0;
	}

	return -1;
}


int pci_read_config_word(const struct pci_dev * dev,int where,u16 * val)
{
	switch (where) {
		case PCI_COMMAND:
			*val = PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO;
			return 0;
	/*
	 * rtlwifi: read but ignored
	 */
	case PCI_INTERRUPT_LINE:
		*val = 0;
		return 0;
	};

	return -1;
}


int pci_read_config_byte(const struct pci_dev * dev,int where,u8 * val)
{
	switch (where) {
	/*
	 * rtlwifi: apparently needed for device distinction
	 */
	case PCI_REVISION_ID:
		*val = dev->revision;
		return 0;
	/*
	 * rtlwifi: needed for enabling DMA 64bit support
	 */
	case 0x719:
		*val = 0;
		return 0;
	/*
	 * rtlwifi: below are registers related to ASPM and PCI link
	 *          control that we do not handle (yet).
	 */
	case 0x80:
	case 0x81:
	case 0x98:
		*val = 0;
		return 0;
	}

	return -1;
}


void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	struct resource *r;
	unsigned long phys_addr;
	unsigned long size;

	if (!dev || bar > 5) {
		printk("%s:%d: invalid request for dev: %p bar: %d\n",
			   __func__, __LINE__, dev, bar);
		return NULL;
	}

	printk("pci_iomap: request for dev: %s bar: %d\n", dev_name(&dev->dev), bar);

	r = &dev->resource[bar];

	phys_addr = r->start;
	size      = r->end - r->start;

	if (!phys_addr || !size)
		return NULL;

	return lx_emul_io_mem_map(phys_addr, size);
}


void __iomem *pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen)
{
	return pci_iomap(pdev, bar, maxlen);
}
