/*
 * \brief  Replaces drivers/pci/pci.c
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/pci.h>
#include <lx_emul/pci.h>

int pci_enable_device(struct pci_dev * dev)
{
	lx_emul_pci_enable(dev_name(&dev->dev));
	return 0;
}


int pcim_enable_device(struct pci_dev *pdev)
{
	/* for now ignore devres */
	return pci_enable_device(pdev);
}


void pci_set_master(struct pci_dev * dev) { }


int pci_set_mwi(struct pci_dev * dev)
{
	return 1;
}


int pci_try_set_mwi(struct pci_dev *dev)
{
	return pci_set_mwi(dev);
}


bool pci_dev_run_wake(struct pci_dev * dev)
{
	return false;
}


u8 pci_find_capability(struct pci_dev * dev,int cap)
{
	return 0;
}


void pci_release_regions(struct pci_dev *pdev) { }


int pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	return 0;
}
