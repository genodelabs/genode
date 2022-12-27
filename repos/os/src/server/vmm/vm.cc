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

#include <fdt.h>
#include <vm.h>

using Vmm::Vm;


enum { LOG2_2MB = 21 };

Genode::Entrypoint & Vm::Cpu_entry::ep(unsigned i, Vm & vm)
{
	if (i == 0)
		return vm._env.ep();

	_ep.construct(vm._env, STACK_SIZE, "vcpu ep",
	              vm._env.cpu().affinity_space().location_of_index(i));
	return *_ep;
}


Vm::Cpu_entry::Cpu_entry(unsigned i, Vm & vm)
:
	cpu(vm, vm._vm, vm._bus, vm._gic, vm._env, vm._heap, ep(i, vm), i) { }


Genode::addr_t Vm::_initrd_offset() const
{
	return align_addr(KERNEL_OFFSET+_kernel_rom.size(), LOG2_2MB);
}


Genode::addr_t Vm::_dtb_offset() const
{
	return align_addr(_initrd_offset()+_initrd_rom.size(), LOG2_2MB);
}


void Vm::_load_kernel()
{
	memcpy((void*)(_ram.local() + KERNEL_OFFSET),
	       _kernel_rom.local_addr<void>(), _kernel_rom.size());
}


void Vm::_load_initrd()
{
	memcpy((void*)(_ram.local() + _initrd_offset()),
	       _initrd_rom.local_addr<void>(), _initrd_rom.size());
}


void Vm::_load_dtb()
{
	Fdt_generator fdt(_env, _heap, _ram.local() + _dtb_offset(), 1 << LOG2_2MB);
	fdt.generate(_config, (void*)(_ram.base()+_initrd_offset()), _initrd_rom.size());
}


Vmm::Cpu & Vm::boot_cpu()
{
	if (!_cpu_list.first()) {
		Cpu_entry * last = nullptr;
		for (unsigned i = 0; i < _config.cpu_count(); i++) {
			Cpu_entry * e = new (_heap) Cpu_entry(i, *this);
			_cpu_list.insert(e, last);
			last = e;
		}
	}

	return _cpu_list.first()->cpu;
}


Vm::Vm(Genode::Env & env, Heap & heap, Config & config)
:
	_env(env),
	_heap(heap),
	_config(config),
	_gic("Gic", GICD_MMIO_START, GICD_MMIO_SIZE,
	     config.cpu_count(), config.gic_version(), _vm, _bus, env),
	_uart("Pl011", PL011_MMIO_START, PL011_MMIO_SIZE,
	      PL011_IRQ, boot_cpu(), _bus, env)
{
	_vm.attach(_vm_ram.cap(), RAM_START,
	           Genode::Vm_session::Attach_attr { .offset     = 0,
	                                             .size       = 0,
	                                             .executable = true,
	                                             .writeable  = true });

	_config.for_each_virtio_device([&] (Config::Virtio_device const & dev) {
		switch (dev.type) {
		case Config::Virtio_device::CONSOLE:
			_device_list.insert(new (_heap)
				Virtio_console(dev.name.string(), (uint64_t)dev.mmio_start,
				               dev.mmio_size, dev.irq, boot_cpu(),
				               _bus, _ram, env));
			return;
		case Config::Virtio_device::NET:
			_device_list.insert(new (_heap)
				Virtio_net(dev.name.string(), (uint64_t)dev.mmio_start,
				           dev.mmio_size, dev.irq, boot_cpu(), _bus, _ram,
				           env));
				return;
		case Config::Virtio_device::BLOCK:
			_device_list.insert(new (_heap)
				Virtio_block_device(dev.name.string(), (uint64_t)dev.mmio_start,
				                    dev.mmio_size, dev.irq, boot_cpu(),
				                    _bus, _ram, env, heap));
		default:
			return;
		};
	});

	_load_kernel();
	_load_initrd();
	_load_dtb();

	Genode::log("Start virtual machine ...");

	Cpu & cpu = boot_cpu();
	cpu.initialize_boot(_ram.base() + KERNEL_OFFSET,
	                    _ram.base() + _dtb_offset());
	cpu.run();
};


Vm::~Vm()
{
	while (_cpu_list.first())    destroy(_heap, _cpu_list.first());
	while (_device_list.first()) destroy(_heap, _device_list.first());
}
