/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <platform_thread.h>
#include <address_space.h>
#include <core_cspace.h>
#include <vm_space.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

namespace Core { class Platform_pd; }


class Core::Platform_pd : public Address_space
{
	public:

		/*
		 * Allocator for core-managed selectors within the PD's CSpace
		 */
		struct Sel_alloc : Bit_allocator<1 << NUM_CORE_MANAGED_SEL_LOG2>
		{
			Sel_alloc() {
				bool ok = _reserve(0, INITIAL_SEL_END);
				ASSERT (ok);
			}
		};

		typedef Bit_allocator<1u << Core_cspace::NUM_TOP_SEL_LOG2> Pd_id_allocator;

	private:

		unsigned _id { };  /* used as index in top-level CNode */

		Page_table_registry _page_table_registry;

		Allocator::Alloc_result _page_directory { };

		Cap_sel _page_directory_sel { 0 };

		Constructible<Vm_space> _vm_space { };

		Constructible<Cnode> _cspace_cnode_1st { };

		Constructible<Cnode> _cspace_cnode_2nd[1UL << CSPACE_SIZE_LOG2_1ST];

		Native_capability _parent { };

		Sel_alloc _sel_alloc { };
		Mutex _sel_alloc_mutex { };

		void _init_page_directory();
		void _deinit_page_directory();

	public:

		/**
		 * Constructors
		 */
		Platform_pd(Allocator &md_alloc, char const *);

		/**
		 * Destructor
		 */
		~Platform_pd();

		/**
		 * Allocate capability selector for threads
		 */
		void alloc_thread_selectors(auto const &fn)
		{
			Mutex::Guard guard(_sel_alloc_mutex);

			fn(_sel_alloc);
		}

		/**
		 * Release capability selector
		 */
		void free_sel(Cap_sel sel)
		{
			Mutex::Guard guard(_sel_alloc_mutex);

			_sel_alloc.free(sel.value());
		}

		/**
		 * Map physical IPC buffer to virtual UTCB address
		 */
		[[nodiscard]] bool map_ipc_buffer(Ipc_buffer_phys const &, Utcb_virt);

		/**
		 * Unmap IPC buffer from PD, at 'Platform_thread' destruction time
		 */
		void unmap_ipc_buffer(Utcb_virt);

		/**
		 * Assign parent interface to protection domain
		 */
		void assign_parent(Native_capability parent);


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t, Core_local_addr) override;


		/*****************************
		 ** seL4-specific interface **
		 *****************************/

		void with_cspace_cnode(Cap_sel sel, auto const &fn)
		{
			const unsigned index = sel.value() / (1 << CSPACE_SIZE_LOG2_2ND);

			if (index >= sizeof(_cspace_cnode_2nd) /
			             sizeof(_cspace_cnode_2nd[0]))
				return;

			if (_cspace_cnode_2nd[index].constructed() &&
			    _cspace_cnode_2nd[index]->constructed())
				fn(*_cspace_cnode_2nd[index]);
			else
				warning(__func__, " invalid sel");
		}

		void with_cspace_cnode_1st(auto const &fn)
		{
			if (_cspace_cnode_1st.constructed() && _cspace_cnode_1st->constructed())
				fn(*_cspace_cnode_1st);
		}

		Cap_sel page_directory_sel() const { return _page_directory_sel; }

		size_t cspace_size_log2() { return CSPACE_SIZE_LOG2; }

		bool install_mapping(Mapping const &mapping, const char * thread_name);

		static Pd_id_allocator &pd_id_alloc();
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
