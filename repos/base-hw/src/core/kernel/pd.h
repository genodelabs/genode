/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__PD_H_
#define _CORE__KERNEL__PD_H_

/* base-hw Core includes */
#include <hw/assert.h>
#include <kernel/core_interface.h>
#include <object.h>
#include <board.h>

/* base includes */
#include <util/reconstructible.h>

namespace Genode { class Platform_pd; }


namespace Kernel {

	class Cpu;

	/**
	 * Kernel backend of protection domains
	 */
	class Pd;
}


class Kernel::Pd
{
	public:

		static constexpr unsigned max_cap_ids = 1 << (sizeof(capid_t) * 8);

		using Capid_allocator = Genode::Bit_allocator<max_cap_ids>;

	private:

		Kernel::Object                 _kernel_object { *this };
		Hw::Page_table                &_table;
		Genode::Platform_pd           &_platform_pd;
		Capid_allocator                _capid_alloc { };
		Object_identity_reference_tree _cap_tree    { };

	public:

		Genode::Cpu::Mmu_context mmu_regs;

		/**
		 * Constructor
		 *
		 * \param table        translation table of the PD
		 * \param platform_pd  core object of the PD
		 */
		Pd(Hw::Page_table                    &table,
		   Genode::Platform_pd               &platform_pd,
		   Board::Address_space_id_allocator &addr_space_id_alloc)
		:
			_table(table),
			_platform_pd(platform_pd),
			mmu_regs((addr_t)&table, addr_space_id_alloc)
		{
			capid_t invalid = (capid_t)_capid_alloc.alloc();
			assert(invalid == cap_id_invalid());
		}

		~Pd()
		{
			while (Object_identity_reference *oir = _cap_tree.first())
				oir->~Object_identity_reference();
		}

		static capid_t syscall_create(Genode::Kernel_object<Pd> &p,
		                              Hw::Page_table            &tt,
		                              Genode::Platform_pd       &pd)
		{
			return (capid_t)call(call_id_new_pd(), (Call_arg)&p,
			                     (Call_arg)&tt, (Call_arg)&pd);
		}

		static void syscall_destroy(Genode::Kernel_object<Pd> & p) {
			call(call_id_delete_pd(), (Call_arg)&p); }

		/**
		 * Check whether the given 'cpu' needs to do some maintainance
		 * work, after this pd has had changes in its page-tables
		 */
		bool invalidate_tlb(Cpu & cpu, addr_t addr, size_t size);


		/***************
		 ** Accessors **
		 ***************/

		Object              &kernel_object()       { return _kernel_object; }
		Genode::Platform_pd &platform_pd()         { return _platform_pd; }
		Hw::Page_table      &translation_table()   { return _table;       }
		Capid_allocator     &capid_alloc()         { return _capid_alloc; }
		Object_identity_reference_tree &cap_tree() { return _cap_tree;    }
};

#endif /* _CORE__KERNEL__PD_H_ */
