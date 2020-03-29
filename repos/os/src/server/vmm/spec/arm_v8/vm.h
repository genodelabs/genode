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

#ifndef _SRC__SERVER__VMM__VM_H_
#define _SRC__SERVER__VMM__VM_H_

#include <ram.h>
#include <exception.h>
#include <cpu.h>
#include <gic.h>
#include <pl011.h>
#include <virtio_console.h>
#include <virtio_net.h>
#include <tester.h>

#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <vm_session/connection.h>

namespace Vmm { class Vm; }

class Vmm::Vm
{
	private:

		using Ep = Genode::Entrypoint;

		enum {
			RAM_ADDRESS   = 0x40000000,
			RAM_SIZE      = 128 * 1024 *1024,
			KERNEL_OFFSET = 0x80000,
			INITRD_OFFSET = 32 * 1024 * 1024,
			DTB_OFFSET    = 64 * 1024 * 1024,
			MAX_CPUS      = 1,
			STACK_SIZE    = sizeof(unsigned long) * 2048,
		};

		Genode::Env                  & _env;
		Genode::Vm_connection          _vm         { _env           };
		Genode::Attached_rom_dataspace _kernel_rom { _env, "linux"  };
		Genode::Attached_rom_dataspace _dtb_rom    { _env, "dtb"    };
		Genode::Attached_rom_dataspace _initrd_rom { _env, "initrd" };
		Genode::Attached_ram_dataspace _vm_ram     { _env.ram(), _env.rm(),
		                                             RAM_SIZE, Genode::CACHED };
		Ram                            _ram        { RAM_ADDRESS, RAM_SIZE,
		                                             (Genode::addr_t)_vm_ram.local_addr<void>()};
		Genode::Heap                   _heap       { _env.ram(), _env.rm() };
		Mmio_bus                       _bus;
		Gic                            _gic;
		Genode::Constructible<Ep>      _eps[MAX_CPUS];
		Genode::Constructible<Cpu>     _cpus[MAX_CPUS];
		Pl011                          _uart;
		Virtio_console                 _virtio_console;
		Virtio_net                     _virtio_net;
		Tester                         _tester;


        void _load_kernel();
		void _load_dtb();
		void _load_initrd();

	public:

		Vm(Genode::Env & env);

		Mmio_bus & bus() { return _bus; }
		Cpu      & boot_cpu();

		template <typename F>
		void cpu(unsigned cpu, F func)
		{
			if (cpu >= MAX_CPUS) Genode::error("Cpu number out of bounds ");
			else                 func(*_cpus[cpu]);
		}

		static unsigned last_cpu() { return MAX_CPUS - 1; }
};

#endif /* _SRC__SERVER__VMM__VM_H_ */
