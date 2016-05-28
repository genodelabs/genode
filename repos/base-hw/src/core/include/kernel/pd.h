/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__KERNEL__PD_H_
#define _CORE__INCLUDE__KERNEL__PD_H_

/* core includes */
#include <translation_table_allocator_tpl.h>
#include <kernel/cpu.h>
#include <kernel/object.h>

namespace Genode {
	class Platform_pd;
}

namespace Kernel
{
	/**
	 * Controls the mode-transition page
	 *
	 * The mode transition page is a small memory region that is mapped by
	 * every PD to the same virtual address. It contains code that acts as a
	 * link between high privileged CPU mode (often called kernel) and low
	 * privileged CPU mode (often called userland). The mode transition
	 * control provides a simple interface to access the code from within
	 * the kernel.
	 */
	class Mode_transition_control;

	/**
	 * Return the system wide mode-transition control
	 */
	Mode_transition_control * mtc();

	/**
	 * Kernel backend of protection domains
	 */
	class Pd;
}

class Kernel::Mode_transition_control
{
	friend class Pd;

	private:

		/*
		 * set the table allocator to the current minimum of bits-of-one-mword
		 * this is a limitation of the Bit_allocator used within this allocator.
		 * actually only one page-mapping is needed by the MTC allocator
		 */
		typedef Genode::Translation_table_allocator_tpl<64> Allocator;
		typedef Genode::Translation_table  Table;

		Allocator   _alloc;
		Table       _table;
		Cpu_context _master;

		/**
		 * Return size of the mode transition
		 */
		static size_t _size();

		/**
		 * Return size of master-context space in the mode transition
		 */
		static size_t _master_context_size();

		/**
		 * Return virtual address of the user entry-code
		 */
		static addr_t _virt_user_entry();

	public:

		enum {
			SIZE       = Cpu::mtc_size,
			VIRT_BASE  = Cpu::exception_entry,
			ALIGN_LOG2 = Genode::Translation_table::ALIGNM_LOG2,
			ALIGN      = 1 << ALIGN_LOG2,
		};

		/**
		 * Constructor
		 *
		 * \param c  CPU context for kernel mode entry
		 */
		Mode_transition_control();

		/**
		 * Map the mode transition page to a virtual address space
		 *
		 * \param tt     translation buffer of the address space
		 * \param alloc  translation table allocator used for the mapping
		 */
		void map(Genode::Translation_table * tt,
		         Genode::Translation_table_allocator * alloc);

		/**
		 * Continue execution of client context
		 *
		 * \param context           targeted CPU context
		 * \param cpu               kernel name of targeted CPU
		 * \param entry_raw         raw pointer to assembly entry-code
		 * \param context_ptr_base  base address of client-context pointer region
		 */
		void switch_to(Cpu::Context * const context,
		               unsigned const cpu,
		               addr_t const entry_raw,
		               addr_t const context_ptr_base);

		/**
		 * Continue execution of user context
		 *
		 * \param context           targeted CPU context
		 * \param cpu               kernel name of targeted CPU
		 */
		 void switch_to_user(Cpu::Context * const context,
		                     unsigned const cpu);
} __attribute__((aligned(Mode_transition_control::ALIGN)));


class Kernel::Pd : public Cpu::Pd,
                   public Kernel::Object
{
	public:

		static constexpr unsigned max_cap_ids = 1 << (sizeof(capid_t) * 8);

		using Table           = Genode::Translation_table;
		using Capid_allocator = Genode::Bit_allocator<max_cap_ids>;

	private:

		Table                  * const _table;
		Genode::Platform_pd    * const _platform_pd;
		Capid_allocator                _capid_alloc;
		Object_identity_reference_tree _cap_tree;

	public:

		/**
		 * Constructor
		 *
		 * \param table        translation table of the PD
		 * \param platform_pd  core object of the PD
		 */
		Pd(Table * const table, Genode::Platform_pd * const platform_pd);

		~Pd();

		/**
		 * Let the CPU context 'c' join the PD
		 */
		void admit(Cpu::Context * const c);


		static capid_t syscall_create(void * const dst,
		                              Genode::Translation_table * tt,
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
		Table               * translation_table() const { return _table;       }
		Capid_allocator     & capid_alloc()             { return _capid_alloc; }
		Object_identity_reference_tree & cap_tree()     { return _cap_tree;    }
};

#endif /* _CORE__INCLUDE__KERNEL__PD_H_ */
