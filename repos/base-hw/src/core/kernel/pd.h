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

/* base-hw core includes */
#include <hw/assert.h>
#include <kernel/core_interface.h>
#include <object.h>
#include <board.h>

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

		/**
		 * The arguments necessary to initialize a Kernel::Pd
		 * via core's pd session service exceeds the typical
		 * few arguments the syscall low-level API supports.
		 * Therefore, we use a compound object to transfer them.
		 */
		struct Core_pd_data {
			addr_t                     table_phys_addr;
			void                      *table;
			Hw::Page_table_translator &table_translator;
			Cap_slab                  &cap_slab;
			const char * const         label;
		};

	private:

		Kernel::Object                 _kernel_object { *this };
		Core_pd_data                   _core_data;
		Capid_allocator                _capid_alloc { };
		Object_identity_reference_tree _cap_tree    { };

	public:

		Board::Cpu::Mmu_context mmu_regs;

		/*
		 * Noncopyable
		 */
		Pd(Pd const &) = delete;
		Pd &operator = (Pd const &) = delete;

		Pd(Core_pd_data &core_pd_data,
		   Board::Address_space_id_allocator &addr_space_id_alloc)
		:
			_core_data(core_pd_data),
			mmu_regs(_core_data.table_phys_addr, addr_space_id_alloc)
		{
			/* exclude invalid ID from allocator */
			assert(_capid_alloc.alloc(cap_id_invalid()).ok());
		}

		~Pd()
		{
			while (Object_identity_reference *oir = _cap_tree.first())
				oir->~Object_identity_reference();
		}

		static capid_t syscall_create(Core::Kernel_object<Pd> &p,
		                              Core_pd_data            &core_data)
		{
			return (capid_t)call(call_id_new_pd(), (Call_arg)&p,
			                     (Call_arg)&core_data);
		}

		static void syscall_destroy(Core::Kernel_object<Pd> &p) {
			call(call_id_delete_pd(), (Call_arg)&p); }

		/**
		 * Check whether the given 'cpu' needs to do some maintainance
		 * work, after this pd has had changes in its page-tables
		 */
		bool invalidate_tlb(Cpu &cpu, addr_t addr, size_t size);

		void with_table(auto const &fn)
		{
			if (_core_data.table)
				fn(*(Hw::Page_table*)(_core_data.table),
				   _core_data.table_translator);
		}


		/***************
		 ** Accessors **
		 ***************/

		Object              &kernel_object()       { return _kernel_object; }
		Capid_allocator     &capid_alloc()         { return _capid_alloc;   }
		Object_identity_reference_tree &cap_tree() { return _cap_tree;      }

		Cap_slab & cap_slab() { return _core_data.cap_slab; }
		char const * label() const { return _core_data.label; }
};

#endif /* _CORE__KERNEL__PD_H_ */
