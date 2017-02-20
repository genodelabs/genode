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

/* local includes */
#include <vm_base.h>
#include <atag.h>

namespace Genode { class Vm; }


class Genode::Vm : public Vm_base
{
	private:

		enum { ATAG_OFFSET = 0x100 };


		/*************
		 ** Vm_base **
		 *************/

		void _load_kernel_surroundings();

		addr_t _board_info_offset() const { return ATAG_OFFSET; }

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

		void on_vmm_exit() { }
		void on_vmm_entry() { };
};

#endif /* _VM_H_ */
