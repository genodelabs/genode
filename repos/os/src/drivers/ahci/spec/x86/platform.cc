/*
 * \brief  Driver for PCI-bus platforms
 * \author Sebastian Sumpf
 * \date   2020-01-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <ahci.h>

Ahci::Data::Data(Env &env)
  : env(env)
{
	pci_device_cap = pci.with_upgrade(
		[&] () { return pci.next_device(pci_device_cap, AHCI_DEVICE,
		                                CLASS_MASK); });

	if (!pci_device_cap.valid()) {
		throw Missing_controller();
	}

	/* construct pci client */
	pci_device.construct(pci_device_cap);
	log("AHCI found ("
	    "vendor: ", Hex(pci_device->vendor_id()), " "
	    "device: ", Hex(pci_device->device_id()), " "
	    "class: ",  Hex(pci_device->class_code()), ")");

	/* map base address of controller */
	Io_mem_session_capability iomem_cap = pci_device->io_mem(pci_device->phys_bar_to_virt(AHCI_BASE_ID));
	iomem.construct(env.rm(), Io_mem_session_client(iomem_cap).dataspace());

	uint16_t cmd = pci_device->config_read(PCI_CMD, ::Platform::Device::ACCESS_16BIT);
	cmd |= 0x2; /* respond to memory space accesses */
	cmd |= 0x4; /* enable bus master */
	_config_write(PCI_CMD, cmd, ::Platform::Device::ACCESS_16BIT);

	irq.construct(pci_device->irq(0));
}


/************************
 ** Platform interface **
 ************************/

Genode::addr_t Ahci::Platform::_mmio_base() const
{
	return addr_t(_data.iomem->local_addr<addr_t>());
}


void Ahci::Platform::sigh_irq(Signal_context_capability sigh)
{
	_data.irq->sigh(sigh);
	ack_irq();
}


void Ahci::Platform::ack_irq() { _data.irq->ack_irq(); }


Genode::Ram_dataspace_capability Ahci::Platform::alloc_dma_buffer(size_t size)
{
	size_t donate = size;

	return retry<Genode::Out_of_ram>(
		[&] () {
			return retry<Genode::Out_of_caps>(
				[&] () { return _data.pci.alloc_dma_buffer(size); },
				[&] () { _data.pci.upgrade_caps(2); });
		},
		[&] () {
			_data.pci.upgrade_ram(donate);
			donate = donate * 2 > size ? 4096 : donate * 2;
		});
}


void Ahci::Platform::free_dma_buffer(Genode::Ram_dataspace_capability ds)
{
	_data.pci.free_dma_buffer(ds);
}
