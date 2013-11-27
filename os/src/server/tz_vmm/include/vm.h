/*
 * \brief  Virtual Machine Monitor VM definition
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__VM_H_
#define _SRC__SERVER__VMM__INCLUDE__VM_H_

/* Genode includes */
#include <base/elf.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <rom_session/connection.h>
#include <vm_session/connection.h>

/* local includes */
#include <mmu.h>
#include <atag.h>

class Vm {

	private:

		enum {
			ATAG_OFFSET   = 0x100,
			INITRD_OFFSET = 0x1000000,
		};

		Genode::Vm_connection     _vm_con;
		Genode::Rom_connection    _kernel_rom;
		Genode::Rom_connection    _initrd_rom;
		Genode::Dataspace_client  _kernel_cap;
		Genode::Dataspace_client  _initrd_cap;
		const char*               _cmdline;
		Vm_state                 *_state;
		Genode::Io_mem_connection _ram_iomem;
		Ram                       _ram;
		Genode::addr_t            _kernel_offset;
		unsigned long             _mach_type;
		unsigned long             _board_rev;

		void _load_kernel()
		{
			using namespace Genode;

			addr_t addr = env()->rm_session()->attach(_kernel_cap);
			memcpy((void*)(_ram.local() + _kernel_offset),
			       (void*)addr, _kernel_cap.size());
			_state->ip = _ram.base() + _kernel_offset;
			env()->rm_session()->detach((void*)addr);
		}

		void _load_initrd()
		{
			using namespace Genode;

			addr_t addr = env()->rm_session()->attach(_initrd_cap);
			memcpy((void*)(_ram.local() + INITRD_OFFSET),
			       (void*)addr, _initrd_cap.size());
			env()->rm_session()->detach((void*)addr);
		}

		void _prepare_atag()
		{
			Atag tag((void*)(_ram.local() + ATAG_OFFSET));
			tag.setup_mem_tag(_ram.base(), _ram.size());
			tag.setup_cmdline_tag(_cmdline);
			tag.setup_initrd2_tag(_ram.base() + INITRD_OFFSET, _initrd_cap.size());
			if (_board_rev)
				tag.setup_rev_tag(_board_rev);
			tag.setup_end_tag();
		}

	public:

		Vm(const char *kernel, const char *initrd, const char *cmdline,
		   Genode::addr_t ram_base, Genode::size_t ram_size,
		   Genode::addr_t kernel_offset, unsigned long mach_type,
		   unsigned long board_rev = 0)
		: _kernel_rom(kernel),
		  _initrd_rom(initrd),
		  _kernel_cap(_kernel_rom.dataspace()),
		  _initrd_cap(_initrd_rom.dataspace()),
		  _cmdline(cmdline),
		  _state((Vm_state*)Genode::env()->rm_session()->attach(_vm_con.cpu_state())),
		  _ram_iomem(ram_base, ram_size),
		  _ram(ram_base, ram_size, (Genode::addr_t)Genode::env()->rm_session()->attach(_ram_iomem.dataspace())),
		  _kernel_offset(kernel_offset),
		  _mach_type(mach_type),
		  _board_rev(board_rev) { }

		void start()
		{
			Genode::memset((void*)_state, 0, sizeof(Vm_state));
			_load_kernel();
			_load_initrd();
			_prepare_atag();
			_state->cpsr = 0x93; /* SVC mode and IRQs disabled */
			_state->r1   = _mach_type;
			_state->r2   = _ram.base() + ATAG_OFFSET; /* ATAG addr */
		}

		void sig_handler(Genode::Signal_context_capability sig_cap) {
			_vm_con.exception_handler(sig_cap); }

		void run()   { _vm_con.run();   }
		void pause() { _vm_con.pause(); }

		void dump()
		{
			using namespace Genode;

			const char * const modes[] =
				{ "und", "svc", "abt", "irq", "fiq" };
			const char * const exc[] =
				{ "invalid", "reset", "undefined", "smc", "pf_abort",
			      "data_abort", "irq", "fiq" };

			printf("Cpu state:\n");
			printf("  Register     Virt     Phys\n");
			printf("---------------------------------\n");
			printf("  r0         = %08lx [%08lx]\n",
			       _state->r0, va_to_pa(_state->r0));
			printf("  r1         = %08lx [%08lx]\n",
			       _state->r1, va_to_pa(_state->r1));
			printf("  r2         = %08lx [%08lx]\n",
			       _state->r2, va_to_pa(_state->r2));
			printf("  r3         = %08lx [%08lx]\n",
			       _state->r3, va_to_pa(_state->r3));
			printf("  r4         = %08lx [%08lx]\n",
			       _state->r4, va_to_pa(_state->r4));
			printf("  r5         = %08lx [%08lx]\n",
			       _state->r5, va_to_pa(_state->r5));
			printf("  r6         = %08lx [%08lx]\n",
			       _state->r6, va_to_pa(_state->r6));
			printf("  r7         = %08lx [%08lx]\n",
			       _state->r7, va_to_pa(_state->r7));
			printf("  r8         = %08lx [%08lx]\n",
			       _state->r8, va_to_pa(_state->r8));
			printf("  r9         = %08lx [%08lx]\n",
			       _state->r9, va_to_pa(_state->r9));
			printf("  r10        = %08lx [%08lx]\n",
			       _state->r10, va_to_pa(_state->r10));
			printf("  r11        = %08lx [%08lx]\n",
			       _state->r11, va_to_pa(_state->r11));
			printf("  r12        = %08lx [%08lx]\n",
			       _state->r12, va_to_pa(_state->r12));
			printf("  sp         = %08lx [%08lx]\n",
			       _state->sp, va_to_pa(_state->sp));
			printf("  lr         = %08lx [%08lx]\n",
			       _state->lr, va_to_pa(_state->lr));
			printf("  ip         = %08lx [%08lx]\n",
			       _state->ip, va_to_pa(_state->ip));
			printf("  cpsr       = %08lx\n", _state->cpsr);
			for (unsigned i = 0;
			     i < Vm_state::Mode_state::MAX; i++) {
				printf("  sp_%s     = %08lx [%08lx]\n", modes[i],
				       _state->mode[i].sp, va_to_pa(_state->mode[i].sp));
				printf("  lr_%s     = %08lx [%08lx]\n", modes[i],
				       _state->mode[i].lr, va_to_pa(_state->mode[i].lr));
				printf("  spsr_%s   = %08lx [%08lx]\n", modes[i],
				       _state->mode[i].spsr, va_to_pa(_state->mode[i].spsr));
			}
			printf("  ttbr0      = %08lx\n", _state->ttbr[0]);
			printf("  ttbr1      = %08lx\n", _state->ttbr[1]);
			printf("  ttbrc      = %08lx\n", _state->ttbrc);
			printf("  dfar       = %08lx [%08lx]\n",
			       _state->dfar, va_to_pa(_state->dfar));
			printf("  exception  = %s\n", exc[_state->cpu_exception]);
		}

		Genode::addr_t va_to_pa(Genode::addr_t va)
		{
			try {
				Mmu mmu(_state, &_ram);
				return mmu.phys_addr(va);
			} catch(Ram::Invalid_addr) {}
			return 0;
		}

		Vm_state *state() const { return  _state; }
		Ram      *ram()         { return &_ram;   }
};

#endif /* _SRC__SERVER__VMM__INCLUDE__VM_H_ */
