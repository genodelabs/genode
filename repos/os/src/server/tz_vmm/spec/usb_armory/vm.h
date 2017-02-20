/*
 * \brief  Virtual Machine implementation
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2015-02-27
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VM_H_
#define _VM_H_

/* Genode includes */
#include <gpio_session/connection.h>

/* local includes */
#include <vm_base.h>

namespace Genode { class Vm; }


class Genode::Vm : public Vm_base
{
	private:

		enum { DTB_OFFSET = 0x1000000 };

		Gpio::Connection  _led { _env, 123 };


		/*************
		 ** Vm_base **
		 *************/

		void _load_kernel_surroundings();

		addr_t _board_info_offset() const { return DTB_OFFSET; }

	public:

		Vm(Env                &env,
		   Kernel_name  const &kernel,
		   Command_line const &cmdl,
		   addr_t              ram,
		   size_t              ram_sz,
		   off_t               kernel_off,
		   Machine_type        mach,
		   Board_revision      board)
		: Vm_base(env, kernel, cmdl, ram, ram_sz, kernel_off, mach, board) { }

		void on_vmm_exit() { _led.write(true); }
		void on_vmm_entry();
};

#endif /* _VM_H_ */
