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
#include <drivers/trustzone.h>

/* local includes */
#include <tsc_380.h>
#include <bp_147.h>
#include <sys_reg.h>
#include <sp810.h>
#include <vm.h>

using namespace Genode;

namespace Vmm {
	class Vmm;
}

class Vmm::Vmm : public Thread<8192>
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

			uint32_t ctrl = _vm->state()->r2;
			uint32_t data = _vm->state()->r0;

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
			switch (_vm->state()->r1) {
			case SP810_ENABLE:
				_sp810.enable_timer0();
				_sp810.enable_timer1();
				break;
			case CPU_ID:
				_vm->state()->r0 = 0x0c000191; // Coretile A9 ID
				break;
			case SYS_COUNTER:
				_vm->state()->r0 = _sys.counter();
				break;
			case MISC_FLAGS:
				_vm->state()->r0 = _sys.misc_flags();
				break;
			case SYS_CTRL:
				_sys_ctrl();
				break;
			case MCI_STATUS:
				_vm->state()->r0 = _sys.mci_status();
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
			_vm->sig_handler(sig_cap);
			_vm->start();

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
		: Thread<8192>("vmm"),
		  _tsc_io_mem(tsc_base,     0x1000),
		  _tpc_io_mem(tpc_base,     0x1000),
		  _sys_io_mem(sys_base,     0x1000),
		  _sp810_io_mem(sp810_base, 0x1000),
		  _tsc((addr_t)env()->rm_session()->attach(_tsc_io_mem.dataspace())),
		  _tpc((addr_t)env()->rm_session()->attach(_tpc_io_mem.dataspace())),
		  _sys((addr_t)env()->rm_session()->attach(_sys_io_mem.dataspace())),
		  _sp810((addr_t)env()->rm_session()->attach(_sp810_io_mem.dataspace())),
		  _vm(vm) { }
};


int main()
{
	enum {
		SYS_VEA9X4_BASE   = 0x10000000,
		SP810_VEA9X4_BASE = 0x10001000,
		TPC_VEA9X4_BASE   = 0x100e6000,
		TSC_VEA9X4_BASE   = 0x100ec000,
		MAIN_MEM_START    = Trustzone::NONSECURE_RAM_BASE,
		MAIN_MEM_SIZE     = Trustzone::NONSECURE_RAM_SIZE,
		KERNEL_OFFSET     = 0x8000,
		MACH_TYPE         = 2272,
	};

	static const char* cmdline = "console=ttyAMA0,115200n8 root=/dev/ram0 lpj=1554432";
	static Vm vm("linux", "initrd.gz", cmdline, MAIN_MEM_START, MAIN_MEM_SIZE,
	             KERNEL_OFFSET, MACH_TYPE);
	static Vmm::Vmm vmm(TSC_VEA9X4_BASE, TPC_VEA9X4_BASE,
	                    SYS_VEA9X4_BASE, SP810_VEA9X4_BASE, &vm);

	PINF("Start virtual machine");
	vmm.start();

	sleep_forever();
	return 0;
}
