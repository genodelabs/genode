/*
 * \brief  PCI-sysfs dummies
 * \author Stefan Kalkowski
 * \date   2022-07-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/pci.h>

static struct attribute *pci_bus_attrs[] = {
    NULL,
};


static const struct attribute_group pci_bus_group = {
    .attrs = pci_bus_attrs,
};


const struct attribute_group *pci_bus_groups[] = {
    &pci_bus_group,
    NULL,
};


static struct attribute *pci_dev_attrs[] = {
    NULL,
};


static const struct attribute_group pci_dev_group = {
    .attrs = pci_dev_attrs,
};


const struct attribute_group *pci_dev_groups[] = {
    &pci_dev_group,
    NULL,
};


