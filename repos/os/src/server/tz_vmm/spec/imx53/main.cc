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
#include <base/env.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <drivers/board_base.h>
#include <drivers/trustzone.h>
#include <vm_state.h>

/* local includes */
#include <vm.h>
#include <m4if.h>
#include <serial.h>
#include <block.h>

using namespace Genode;

enum {
	KERNEL_OFFSET    = 0x8000,
	MACH_TYPE_TABLET = 3011,
	MACH_TYPE_QSB    = 3273,
	BOARD_REV_TABLET = 0x53321,
};


static const char* cmdline_tablet = "console=ttymxc0,115200";

void on_vmm_entry();
void on_vmm_exit();

namespace Vmm {
	class Vmm;
}


class Vmm::Vmm : public Thread_deprecated<8192>
{
	private:

		Signal_receiver           _sig_rcv;
		Signal_context            _vm_context;
		Vm                       *_vm;
		Io_mem_connection         _m4if_io_mem;
		M4if                      _m4if;
		Serial                    _serial;
		Block                     _block;

		void _handle_hypervisor_call()
		{
			enum {
				FRAMEBUFFER = 0,
				INPUT       = 1,
				SERIAL      = 2,
				BLOCK       = 3,
			};
			switch (_vm->smc_arg_0()) {
			case FRAMEBUFFER:                      break;
			case INPUT:                            break;
			case SERIAL:      _serial.handle(_vm); break;
			case BLOCK:       _block.handle(_vm);  break;
			default:
				Genode::error("unknown hypervisor call!");
				_vm->dump();
			};
		}

		bool _handle_data_abort()
		{
			_vm->dump();
			return false;
		}

		bool _handle_vm()
		{
			/* check exception reason */
			switch (_vm->state()->cpu_exception) {
			case Cpu_state::DATA_ABORT:
				if (!_handle_data_abort()) {
					Genode::error("could not handle data-abort will exit!");
					return false;
				}
				break;
			case Cpu_state::SUPERVISOR_CALL:
				_handle_hypervisor_call();
				break;
			default:
				Genode::error("curious exception occured");
				_vm->dump();
				return false;
			}
			return true;
		}

	protected:

		void entry()
		{
			_vm->sig_handler(_sig_rcv.manage(&_vm_context));
			_vm->start();
			_vm->run();

			while (true) {
				Signal s = _sig_rcv.wait_for_signal();
				on_vmm_entry();
				if (s.context() == &_vm_context) {
					if (_handle_vm())
						_vm->run();
				} else {
					Genode::warning("invalid context");
					continue;
				}
				on_vmm_exit();
			}
		};

	public:

		Vmm(Vm *vm)
		: Thread_deprecated<8192>("vmm"),
		  _vm(vm),
		  _m4if_io_mem(Board_base::M4IF_BASE, Board_base::M4IF_SIZE),
		  _m4if((addr_t)env()->rm_session()->attach(_m4if_io_mem.dataspace()))
		{
			_m4if.set_region0(Trustzone::SECURE_RAM_BASE,
			                  Trustzone::SECURE_RAM_SIZE);
		}
};


int main()
{
	static Vm vm("linux", cmdline_tablet,
	             Trustzone::NONSECURE_RAM_BASE, Trustzone::NONSECURE_RAM_SIZE,
	             KERNEL_OFFSET, MACH_TYPE_QSB);
	static Vmm::Vmm vmm(&vm);

	Genode::log("Start virtual machine ...");
	vmm.start();

	sleep_forever();
	return 0;
}
