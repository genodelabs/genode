/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Josef Soentgen
 * \date   2022-01-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

const struct attribute_group pci_dev_acpi_attr_group;


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/property.h>

struct device_driver;


struct kobj_uevent_env;


bool pciehp_is_native(struct pci_dev *bridge)
{
	lx_emul_trace_and_stop(__func__);
}

#include <linux/pci.h>

struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus)
{
	lx_emul_trace(__func__);
	return NULL;
}


int usb_acpi_register(void)
{
	lx_emul_trace(__func__);
	return 0;
};


void usb_acpi_unregister(void)
{
	lx_emul_trace(__func__);
};


struct usb_device;
bool usb_acpi_power_manageable(struct usb_device *hdev, int index)
{
	lx_emul_trace(__func__);
	return false;
}


int usb_acpi_set_power_state(struct usb_device *hdev, int index, bool enable)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace(__func__);
	/* suppress */
	return 0;
}
