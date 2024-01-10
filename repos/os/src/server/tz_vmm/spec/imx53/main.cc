/*
 * \brief  Virtual Machine Monitor
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \author Benjamin Lamowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <drivers/defs/imx53.h>
#include <drivers/defs/imx53_trustzone.h>

/* local includes */
#include <vm.h>
#include <m4if.h>
#include <serial_driver.h>
#include <block_driver.h>

using namespace Genode;


class Main
{
	private:

		enum {
			KERNEL_OFFSET  = 0x8000,
			MACHINE_TABLET = 3011,
			MACHINE_QSB    = 3273,
			BOARD_TABLET   = 0x53321,
			BOARD_QSB      = 0,
		};

		Env                    &_env;
		Vm::Kernel_name  const  _kernel_name       { "linux" };
		Vm::Command_line const  _cmd_line          { "console=ttymxc0,115200" };
		Attached_rom_dataspace  _config            { _env, "config" };
		Vcpu_handler<Main>      _exception_handler { _env.ep(), *this,
		                                             &Main::_handle_exception };

		Heap          _heap    { &_env.ram(), &_env.rm() };
		Vm            _vm      { _env, _kernel_name, _cmd_line,
		                         Trustzone::NONSECURE_RAM_BASE,
		                         Trustzone::NONSECURE_RAM_SIZE,
		                         KERNEL_OFFSET, Machine_type(MACHINE_QSB),
		                         Board_revision(BOARD_QSB),
		                         _heap, _exception_handler };
		M4if          _m4if    { _env, {(char *)Imx53::M4IF_BASE, Imx53::M4IF_SIZE} };
		Serial_driver _serial  { _env.ram(), _env.rm() };
		Block_driver  _block   { _env, _config.xml(), _heap, _vm };

		void _handle_smc(Vcpu_state &state)
		{
			enum {
				FRAMEBUFFER = 0,
				INPUT       = 1,
				SERIAL      = 2,
				BLOCK       = 3,
			};
			switch (state.r0) {
			case FRAMEBUFFER:                          break;
			case INPUT:                                break;
			case SERIAL:      _serial.handle_smc(_vm, state); break;
			case BLOCK:       _block.handle_smc(_vm, state);  break;
			default:
				error("unknown hypervisor call ", state.r0);
				throw Vm::Exception_handling_failed();
			};
		}

		void _handle_data_abort()
		{
			error("failed to handle data abort");
			throw Vm::Exception_handling_failed();
		}

		void _handle_exception()
		{
			_vm.with_state([this](Vcpu_state &state) {
				_vm.on_vmm_entry();
				try {
					switch (state.cpu_exception) {
					case Cpu_state::DATA_ABORT:      _handle_data_abort(); break;
					case Cpu_state::SUPERVISOR_CALL: _handle_smc(state);   break;
					case VCPU_EXCEPTION_STARTUP:     _vm.start(state);     break;
					default:
						error("unknown exception ", state.cpu_exception);
						throw Vm::Exception_handling_failed();
					}
				}
				catch (Vm::Exception_handling_failed) { _vm.dump(state); }
				_vm.on_vmm_exit();
				return true;
			});
		};

	public:

		Main(Env &env) : _env(env)
		{
			log("Start virtual machine ...");
			_m4if.set_region0(Trustzone::SECURE_RAM_BASE,
			                  Trustzone::SECURE_RAM_SIZE);
		}
};


void Component::construct(Env &env) { static Main main(env); }
