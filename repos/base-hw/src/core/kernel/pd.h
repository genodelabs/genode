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

/* core includes */
#include <hw/assert.h>
#include <cpu.h>
#include <kernel/core_interface.h>
#include <kernel/object.h>
#include <translation_table.h>

namespace Genode {
	class Platform_pd;
}

namespace Kernel
{
	/**
	 * Kernel backend of protection domains
	 */
	class Pd;
}


class Kernel::Pd : public Kernel::Object
{
	public:

		static constexpr unsigned max_cap_ids = 1 << (sizeof(capid_t) * 8);

		using Capid_allocator = Genode::Bit_allocator<max_cap_ids>;

	private:

		/*
		 * Noncopyable
		 */
		Pd(Pd const &);
		Pd &operator = (Pd const &);

		Hw::Page_table         * const _table;
		Genode::Platform_pd    * const _platform_pd;
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
		Pd(Hw::Page_table * const table,
		   Genode::Platform_pd * const platform_pd)
		: _table(table), _platform_pd(platform_pd),
		  mmu_regs((addr_t)table)
		{
			capid_t invalid = _capid_alloc.alloc();
			assert(invalid == cap_id_invalid());
		}

		~Pd()
		{
			while (Object_identity_reference *oir = _cap_tree.first())
				oir->~Object_identity_reference();
		}


		static capid_t syscall_create(void * const dst,
		                              Hw::Page_table * tt,
		                              Genode::Platform_pd * const pd)
		{
			return call(call_id_new_pd(), (Call_arg)dst,
			            (Call_arg)tt, (Call_arg)pd);
		}

		static void syscall_destroy(Pd * const pd) {
			call(call_id_delete_pd(), (Call_arg)pd); }


		/***************
		 ** Accessors **
		 ***************/

		Genode::Platform_pd * platform_pd()       const { return _platform_pd; }
		Hw::Page_table      * translation_table() const { return _table;       }
		Capid_allocator     & capid_alloc()             { return _capid_alloc; }
		Object_identity_reference_tree & cap_tree()     { return _cap_tree;    }
};

#endif /* _CORE__KERNEL__PD_H_ */
