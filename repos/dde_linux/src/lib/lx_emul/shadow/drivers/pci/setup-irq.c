/*
 * \brief  PCI initialization
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
#include <linux/irq.h>

extern struct irq_chip dde_irqchip_data_chip;


void pci_assign_irq(struct pci_dev * dev)
{
	struct irq_data *irq_data;

	/*
	 * Be lazy and treat irq as hwirq as this is used by the
	 * dde_irqchip_data_chip for (un-)masking.
	 */
	irq_data = irq_get_irq_data(dev->irq);

	irq_data->hwirq = dev->irq;

	irq_set_chip_and_handler(dev->irq, &dde_irqchip_data_chip,
	                         handle_level_irq);
}
