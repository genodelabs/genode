/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <platform_thread.h>
#include <address_space.h>
#include <vm_space.h>

/* base-internal includes */
#include <internal/capability_space_sel4.h>

namespace Genode { class Platform_pd; }


class Genode::Platform_pd : public Address_space
{
	private:

		unsigned const _id;  /* used as index in top-level CNode */

		Page_table_registry _page_table_registry;

		unsigned const _vm_pad_cnode_sel;

		unsigned const _vm_cnode_sel;

		unsigned const        _page_directory_sel;
		Untyped_address  _init_page_directory();
		Untyped_address const _page_directory = _init_page_directory();

		Vm_space _vm_space;

		unsigned const _cspace_cnode_sel;

		Cnode _cspace_cnode;

		Native_capability _parent;

		/*
		 * Allocator for core-managed selectors within the PD's CSpace
		 */
		struct Sel_alloc : Bit_allocator<1 << NUM_CORE_MANAGED_SEL_LOG2>
		{
			Sel_alloc() { _reserve(0, INITIAL_SEL_END); }
		};

		Sel_alloc _sel_alloc;
		Lock _sel_alloc_lock;

	public:

		/**
		 * Constructors
		 */
		Platform_pd(Allocator * md_alloc, char const *,
		            signed pd_id = -1, bool create = true);

		/**
		 * Destructor
		 */
		~Platform_pd();

		/**
		 * Bind thread to protection domain
		 *
		 * \return  0  on success or
		 *         -1  if thread ID allocation failed.
		 */
		int bind_thread(Platform_thread *thread);

		/**
		 * Unbind thread from protection domain
		 *
		 * Free the thread's slot and update thread object.
		 */
		void unbind_thread(Platform_thread *thread);

		/**
		 * Assign parent interface to protection domain
		 */
		int assign_parent(Native_capability parent);


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t);


		/*****************************
		 ** seL4-specific interface **
		 *****************************/

		unsigned alloc_sel();

		void free_sel(unsigned sel);

		Cnode &cspace_cnode() { return _cspace_cnode; }

		unsigned page_directory_sel() const { return _page_directory_sel; }

		size_t cspace_size_log2() { return CSPACE_SIZE_LOG2; }

		void install_mapping(Mapping const &mapping);
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
