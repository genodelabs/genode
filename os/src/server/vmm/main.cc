/*
 * \brief  Virtual Machine Monitor
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/elf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <cpu/cpu_state.h>
#include <io_mem_session/connection.h>
#include <rom_session/connection.h>
#include <vm_session/connection.h>
#include <dataspace/client.h>

/* local includes */
#include <tsc_380.h>
#include <bp_147.h>
#include <sys_reg.h>
#include <sp810.h>
#include <atag.h>

namespace Genode {

	class Ram {

		private:

			addr_t _base;
			size_t _size;
			addr_t _local;

		public:

			Ram(addr_t addr, size_t sz)
			: _base(addr), _size(sz), _local(0) { }

			addr_t base()  { return _base;  }
			size_t size()  { return _size;  }
			addr_t local() { return _local; }

			void attach(Dataspace_capability cap) {
				_local = (addr_t) env()->rm_session()->attach(cap); }
	};


	class Vm {

		private:

			enum {
				ATAG_OFFSET   = 0x100,
				INITRD_OFFSET = 0x800000
			};

			Vm_connection     _vm_con;
			Rom_connection    _elf_rom;
			Rom_connection    _initrd_rom;
			const char*       _cmdline;
			size_t            _initrd_size;
			Cpu_state_modes  *_state;
			Ram               _ram;
			Io_mem_connection _ram_iomem;

			void _load_elf()
			{
				/* attach ELF locally */
				addr_t elf_addr = env()->rm_session()->attach(_elf_rom.dataspace());

				/* setup ELF object and read program entry pointer */
				Elf_binary elf((addr_t)elf_addr);
				_state->ip = elf.entry();
				if (!elf.valid()) {
					PWRN("Invalid elf binary!");
					return;
				}

				Elf_segment seg;
				for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {
					if (seg.flags().skip) continue;

					addr_t addr  = (addr_t)seg.start();
					size_t size  = seg.mem_size();

					if (addr < _ram.base() ||
						(addr + size) > (_ram.base() + _ram.size())) {
						PWRN("Elf binary doesn't fit into RAM");
						return;
					}

					void  *base  = (void*) (_ram.local() + (addr - _ram.base()));
					addr_t laddr = elf_addr + seg.file_offset();

					/* copy contents */
					memcpy(base, (void *)laddr, seg.file_size());

					/* if writeable region potentially fill with zeros */
					if (size > seg.file_size() && seg.flags().w)
						memset((void *)((addr_t)base + seg.file_size()),
						       0, size - seg.file_size());
				}

				/* detach ELF */
				env()->rm_session()->detach((void*)elf_addr);
			}

			void _load_initrd()
			{
				addr_t addr = env()->rm_session()->attach(_initrd_rom.dataspace());
				memcpy((void*)(_ram.local() + INITRD_OFFSET),
				       (void*)addr, _initrd_size);
				env()->rm_session()->detach((void*)addr);
			}

			void _prepare_atag()
			{
				Atag tag((void*)(_ram.local() + ATAG_OFFSET));
				tag.setup_mem_tag(_ram.base(), _ram.size());
				tag.setup_cmdline_tag(_cmdline);
				tag.setup_initrd2_tag(_ram.base() + INITRD_OFFSET, _initrd_size);
				tag.setup_end_tag();
			}

		public:

			Vm(const char *kernel, const char *initrd, const char *cmdline,
			   addr_t ram_base, size_t ram_size)
			: _elf_rom(kernel),
			  _initrd_rom(initrd),
			  _cmdline(cmdline),
			  _initrd_size(Dataspace_client(_initrd_rom.dataspace()).size()),
			  _state((Cpu_state_modes*)env()->rm_session()->attach(_vm_con.cpu_state())),
			  _ram(ram_base, ram_size),
			  _ram_iomem(ram_base, ram_size)
			{
				memset((void*)_state, 0, sizeof(Cpu_state_modes));
				_ram.attach(_ram_iomem.dataspace());
			}

			void start(Signal_context_capability sig_cap)
			{
				_load_elf();
				_load_initrd();
				_prepare_atag();
				_state->cpsr = 0x93; /* SVC mode and IRQs disabled */
				_state->r[1] = 2272; /* MACH_TYPE vexpress board   */
				_state->r[2] = _ram.base() + ATAG_OFFSET; /* ATAG addr */
				_vm_con.exception_handler(sig_cap);
			}

			void run() { _vm_con.run(); }

			void dump()
			{
				const char * const modes[] =
					{ "und", "svc", "abt", "irq", "fiq" };
				const char * const exc[] =
					{ "reset", "undefined", "smc", "pf_abort",
				      "data_abort", "irq", "fiq" };

				printf("Cpu state:\n");
				for (unsigned i = 0; i<13; i++)
					printf("  r%x        = %08lx\n", i, _state->r[i]);
				printf("  sp        = %08lx\n", _state->sp);
				printf("  lr        = %08lx\n", _state->lr);
				printf("  ip        = %08lx\n", _state->ip);
				printf("  cpsr      = %08lx\n", _state->cpsr);
				for (unsigned i = 0;
				     i < Cpu_state_modes::Mode_state::MAX; i++) {
					printf("  sp_%s    = %08lx\n", modes[i], _state->mode[i].sp);
					printf("  lr_%s    = %08lx\n", modes[i], _state->mode[i].lr);
					printf("  spsr_%s  = %08lx\n", modes[i], _state->mode[i].spsr);
				}
				printf("  exception = %s\n", exc[_state->cpu_exception]);
			}

			Cpu_state_modes *state() const { return _state; }
	};


	class Vmm : public Genode::Thread<8192>
	{
		private:

			enum Hypervisor_calls {
				SP810_ENABLE = 1,
				CPU_ID,
				SYS_COUNTER,
				MISC_FLAGS,
				SYS_CTRL,
				MCI_STATUS
			};

			Io_mem_connection _tsc_io_mem;
			Io_mem_connection _tpc_io_mem;
			Io_mem_connection _sys_io_mem;
			Io_mem_connection _sp810_io_mem;

			Tsc_380 _tsc;
			Bp_147  _tpc;
			Sys_reg _sys;
			Sp810   _sp810;

			Vm     *_vm;

			void _sys_ctrl()
			{
				enum {
					OSC1     = 0xc0110001,
					DVI_SRC  = 0xc0710000,
					DVI_MODE = 0xc0b00000
				};

				uint32_t ctrl = _vm->state()->r[2];
				uint32_t data = _vm->state()->r[0];

				switch(ctrl) {
				case OSC1:
					_sys.osc1(data);
					break;
				case DVI_SRC:
					_sys.dvi_source(data);
					break;
				case DVI_MODE:
					_sys.dvi_mode(data);
					break;
				default:
					PWRN("Access violation to sys configuration ctrl=%ux", ctrl);
					_vm->dump();
				}
			}

			void _handle_hypervisor_call()
			{
				switch (_vm->state()->r[1]) {
				case SP810_ENABLE:
					_sp810.enable_timer0();
					_sp810.enable_timer1();
					break;
				case CPU_ID:
					_vm->state()->r[0] = 0x0c000191; // Coretile A9 ID
					break;
				case SYS_COUNTER:
					_vm->state()->r[0] = _sys.counter();
					break;
				case MISC_FLAGS:
					_vm->state()->r[0] = _sys.misc_flags();
					break;
				case SYS_CTRL:
					_sys_ctrl();
					break;
				case MCI_STATUS:
					_vm->state()->r[0] = _sys.mci_status();
					break;
				default:
					PERR("Unknown hypervisor call!");
					_vm->dump();
				}
			}

			bool _handle_data_abort()
			{
				PWRN("Vm tried to access %p which isn't allowed",
				     _tsc.last_failed_access());
				_vm->dump();
				return false;
			}

			bool _handle_vm()
			{
				switch (_vm->state()->cpu_exception) {
				case Cpu_state::DATA_ABORT:
					if (!_handle_data_abort()) {
						PERR("Could not handle data-abort will exit!");
						return false;
					}
					break;
				case Cpu_state::SUPERVISOR_CALL:
					_handle_hypervisor_call();
					break;
				default:
					PERR("Curious exception occured");
					_vm->dump();
					return false;
				}
				return true;
			}

		protected:

			void entry()
			{
				Signal_receiver sig_rcv;
				Signal_context  sig_cxt;
				Signal_context_capability sig_cap(sig_rcv.manage(&sig_cxt));
				_vm->start(sig_cap);
				while (true) {
					_vm->run();
					Signal s = sig_rcv.wait_for_signal();
					if (s.context() != &sig_cxt) {
						PWRN("Invalid context");
						continue;
					}
					if (!_handle_vm())
						return;
				}
			};

		public:

			Vmm(addr_t tsc_base, addr_t tpc_base,
				addr_t sys_base, addr_t sp810_base,
				Vm    *vm)
			: _tsc_io_mem(tsc_base,     0x1000),
			  _tpc_io_mem(tpc_base,     0x1000),
			  _sys_io_mem(sys_base,     0x1000),
			  _sp810_io_mem(sp810_base, 0x1000),
			  _tsc((addr_t)env()->rm_session()->attach(_tsc_io_mem.dataspace())),
			  _tpc((addr_t)env()->rm_session()->attach(_tpc_io_mem.dataspace())),
			  _sys((addr_t)env()->rm_session()->attach(_sys_io_mem.dataspace())),
			  _sp810((addr_t)env()->rm_session()->attach(_sp810_io_mem.dataspace())),
			  _vm(vm) { }
	};
}


int main()
{
	enum {
		SYS_VEA9X4_BASE   = 0x10000000,
		SP810_VEA9X4_BASE = 0x10001000,
		TPC_VEA9X4_BASE   = 0x100e6000,
		TSC_VEA9X4_BASE   = 0x100ec000,
		MAIN_MEM_START    = 0x80000000,
		MAIN_MEM_SIZE     = 0x10000000
	};

	static const char* cmdline = "console=ttyAMA0,38400n8 root=/dev/ram0 lpj=1554432";
	static Genode::Vm  vm("linux", "initrd.gz", cmdline,
	                      MAIN_MEM_START, MAIN_MEM_SIZE);
	static Genode::Vmm vmm(TSC_VEA9X4_BASE, TPC_VEA9X4_BASE,
	                       SYS_VEA9X4_BASE, SP810_VEA9X4_BASE,
	                       &vm);

	PINF("Start virtual machine");
	vmm.start();

	Genode::sleep_forever();
	return 0;
}
