/*
 * \brief  Virtual Machine Monitor VM definition
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VM_BASE_H_
#define _VM_BASE_H_

/* Genode includes */
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <vm_session/connection.h>
#include <util/noncopyable.h>

/* local includes */
#include <vm_state.h>
#include <ram.h>

namespace Genode
{
	class Board_revision;
	class Vm_base;
	class Machine_type;
}

struct Genode::Board_revision
{
	unsigned long value;

	explicit Board_revision(unsigned long value) : value(value) { }
};


struct Genode::Machine_type
{
	unsigned long value;

	explicit Machine_type(unsigned long value) : value(value) { }
};


class Genode::Vm_base : Noncopyable
{
	public:

		using Kernel_name  = String<32>;
		using Command_line = String<64>;

	protected:

		Env                  &_env;
		Kernel_name    const &_kernel;
		Command_line   const &_cmdline;
		off_t          const  _kernel_off;
		Machine_type   const  _machine;
		Board_revision const  _board;
		Ram            const  _ram;
		Vm_connection         _vm    { _env };
		Vm_state             &_state { *(Vm_state*)_env.rm()
		                                           .attach(_vm.cpu_state()) };

		void _load_kernel();

		virtual void   _load_kernel_surroundings() = 0;
		virtual addr_t _board_info_offset() const  = 0;

	public:

		struct Inject_irq_failed         : Exception { };
		struct Exception_handling_failed : Exception { };

		Vm_base(Env                &env,
		        Kernel_name  const &kernel,
		        Command_line const &cmdline,
		        addr_t              ram_base,
		        size_t              ram_size,
		        off_t               kernel_off,
		        Machine_type        machine,
		        Board_revision      board);

		void exception_handler(Signal_context_capability handler) {
			_vm.exception_handler(handler); }

		void run()   { _vm.run();   }
		void pause() { _vm.pause(); }

		void   start();
		void   dump();
		void   inject_irq(unsigned irq);
		addr_t va_to_pa(addr_t va);

		Vm_state const &state() const { return _state; }
		Ram      const &ram()   const { return _ram;   }

		addr_t smc_arg_0() { return _state.r0; }
		addr_t smc_arg_1() { return _state.r1; }
		addr_t smc_arg_2() { return _state.r2; }
		addr_t smc_arg_3() { return _state.r3; }
		addr_t smc_arg_4() { return _state.r4; }
		addr_t smc_arg_5() { return _state.r5; }
		addr_t smc_arg_6() { return _state.r6; }
		addr_t smc_arg_7() { return _state.r7; }
		addr_t smc_arg_8() { return _state.r8; }
		addr_t smc_arg_9() { return _state.r9; }

		void smc_ret(addr_t const ret_0) { _state.r0 = ret_0; }
		void smc_ret(addr_t const ret_0, addr_t const ret_1);
};

#endif /* _VM_BASE_H_ */
