/*
 * \brief  Generic and simple virtio device
 * \author Sebastian Sumpf
 * \date   2019-10-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu.h>
#include <virtio_device.h>

#include <base/log.h>

using Vmm::Virtio_device;
using Register = Vmm::Mmio_register::Register;

Virtio_device::Virtio_device(const char * const     name,
                             const Genode::uint64_t addr,
                             const Genode::uint64_t size,
                             unsigned               irq,
                             Cpu                   &cpu,
                             Mmio_bus              &bus,
                             Ram                   &ram,
                             unsigned               queue_size)
: Mmio_device(name, addr, size),
  _irq(cpu.gic().irq(irq)),
  _ram(ram)
{
	for (unsigned i = 0; i < (sizeof(Dummy::regs) / sizeof(Mmio_register)); i++)
		add(_reg_container.regs[i]);

	add(_device_features);
	add(_driver_features);
	add(_queue_sel);
	add(_queue_ready);
	add(_queue_num);
	add(_queue_notify);
	add(_queue_descr_low);
	add(_queue_descr_high);
	add(_queue_driver_low);
	add(_queue_driver_high);
	add(_queue_device_low);
	add(_queue_device_high);
	add(_interrupt_status);
	add(_interrupt_ack);
	add(_status);

	/* set queue size */
	_reg_container.regs[6].set(queue_size);

	bus.add(*this);
}
