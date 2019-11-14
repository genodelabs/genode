/*
 * \brief  VMM example for ARMv8 virtualization
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vm.h>

using Vmm::Vm;

void Vm::_load_kernel()
{
	Genode::memcpy((void*)(_ram.local() + KERNEL_OFFSET),
	               _kernel_rom.local_addr<void>(),
	               _kernel_rom.size());
}

void Vm::_load_dtb()
{
	Genode::memcpy((void*)(_ram.local() + DTB_OFFSET),
	               _dtb_rom.local_addr<void>(),
	               _dtb_rom.size());
}


void Vm::_load_initrd()
{
	Genode::memcpy((void*)(_ram.local() + INITRD_OFFSET),
	               _initrd_rom.local_addr<void>(),
	               _initrd_rom.size());
}


Vmm::Cpu & Vm::boot_cpu()
{
	if (!_cpus[0].constructed())
		_cpus[0].construct(*this, _vm, _bus, _gic, _env, _heap, _env.ep());
	return *_cpus[0];
}


Vm::Vm(Genode::Env & env)
: _env(env),
  _gic("Gicv3", GICD_MMIO_START, GICD_MMIO_SIZE,
       MAX_CPUS, GIC_VERSION, _vm, _bus, env),
  _uart("Pl011", PL011_MMIO_START, PL011_MMIO_SIZE,
        PL011_IRQ, boot_cpu(), _bus, env),
  _virtio_console("HVC", VIRTIO_CONSOLE_MMIO_START, VIRTIO_CONSOLE_MMIO_SIZE,
                  VIRTIO_CONSOLE_IRQ, boot_cpu(), _bus, _ram, env),
  _virtio_net("Net", VIRTIO_NET_MMIO_START, VIRTIO_NET_MMIO_SIZE,
              VIRTIO_NET_IRQ, boot_cpu(), _bus, _ram, env)
{
	_vm.attach(_vm_ram.cap(), RAM_START);

	_load_kernel();
	_load_dtb();
	_load_initrd();

	for (unsigned i = 1; i < MAX_CPUS; i++) {
		Genode::Affinity::Space space = _env.cpu().affinity_space();
		Genode::Affinity::Location location(space.location_of_index(i));
		_eps[i].construct(_env, STACK_SIZE, "vcpu ep", location);
		_cpus[i].construct(*this, _vm, _bus, _gic, _env, _heap, *_eps[i]);
	}

	Genode::log("Start virtual machine ...");

	Cpu & cpu = boot_cpu();
	cpu.initialize_boot(_ram.base() + KERNEL_OFFSET,
	                    _ram.base() + DTB_OFFSET);
	cpu.run();
};
