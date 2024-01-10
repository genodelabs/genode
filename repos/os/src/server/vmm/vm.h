/*
 * \brief  VMM example for ARMv8 virtualization
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__VM_H_
#define _SRC__SERVER__VMM__VM_H_

#include <board.h>
#include <config.h>
#include <ram.h>
#include <exception.h>
#include <cpu.h>
#include <gic.h>
#include <pl011.h>
#include <virtio_device.h>

#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <gui_session/connection.h>
#include <vm_session/connection.h>

namespace Vmm {
	class Vm;
	using namespace Genode;
}

class Vmm::Vm
{
	private:

		struct Cpu_entry : List<Cpu_entry>::Element
		{
			enum { STACK_SIZE = sizeof(unsigned long) * 2048, };

			Constructible<Entrypoint> _ep {};

			Entrypoint & ep(unsigned i, Vm & vm);
			Cpu          cpu;

			Cpu_entry(unsigned idx, Vm & vm);
		};

		Env                    & _env;
		Heap                   & _heap;
		Config                 & _config;
		Vm_connection            _vm         { _env           };
		Attached_rom_dataspace   _kernel_rom { _env, _config.kernel_name() };
		Attached_ram_dataspace   _vm_ram     { _env.ram(), _env.rm(),
		                                       _config.ram_size(), CACHED };
		Ram                      _ram        { RAM_START, {_vm_ram.local_addr<char>(), _config.ram_size()}};
		Mmio_bus                 _bus {};
		Gic                      _gic;
		List<Cpu_entry>          _cpu_list {};
		List<Virtio_device_base> _device_list {};
		Pl011                    _uart;

		Constructible<Attached_rom_dataspace> _initrd_rom {};
		Constructible<Gui::Connection>        _gui {};

		addr_t _initrd_offset() const;
		size_t _initrd_size()   const;
		addr_t _dtb_offset()    const;

		void _load_kernel();
		void _load_initrd();
		void _load_dtb();

	public:

		Vm(Env & env, Heap & heap, Config & config);
		~Vm();

		Mmio_bus & bus() { return _bus; }
		Cpu      & boot_cpu();

		template <typename F>
		void cpu(unsigned cpu, F func)
		{
			for (Cpu_entry * ce = _cpu_list.first(); ce; ce = ce->next())
				if (ce->cpu.cpu_id() == cpu) func(ce->cpu);
		}

		template <typename F>
		void for_each_cpu(F func)
		{
			for (Cpu_entry * ce = _cpu_list.first(); ce; ce = ce->next())
				func(ce->cpu);
		}

		addr_t dtb_addr() {
			return _ram.guest_base() + _dtb_offset();
		}

		addr_t kernel_addr() {
			return _ram.guest_base() + KERNEL_OFFSET;
		}
};

#endif /* _SRC__SERVER__VMM__VM_H_ */
