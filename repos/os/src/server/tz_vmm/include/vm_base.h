/*
 * \brief  Virtual Machine Monitor VM definition
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \author Benjamin Lamowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2023 Genode Labs GmbH
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
#include <vm_session/handler.h>
#include <util/noncopyable.h>
#include <cpu/vcpu_state_trustzone.h>

/* local includes */
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


class Genode::Vm_base : Noncopyable, Interface
{
	public:

		using Kernel_name  = String<32>;
		using Command_line = String<64>;

	private:

		Vm_base(Vm_base const &);
		Vm_base &operator = (Vm_base const &);

	protected:

		Env                  &_env;
		Kernel_name    const &_kernel;
		Command_line   const &_cmdline;
		off_t          const  _kernel_off;
		Machine_type   const  _machine;
		Board_revision const  _board;
		Ram            const  _ram;

		Vm_connection               _vm          { _env };
		Vm_connection::Exit_config  _exit_config { };
		Vm_connection::Vcpu         _vcpu;

		void _load_kernel(Vcpu_state &);

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
		        Board_revision      board,
		        Allocator          &alloc,
		        Vcpu_handler_base  &handler);

		void   start(Vcpu_state &);
		void   dump(Vcpu_state &);
		void   inject_irq(unsigned irq);
		addr_t va_to_pa(Vcpu_state &state, addr_t va);

		Ram      const &ram()   const { return _ram;   }

		template<typename FN>
		void with_state(FN const & fn)
		{
			_vcpu.with_state(fn);
		}
};

#endif /* _VM_BASE_H_ */
