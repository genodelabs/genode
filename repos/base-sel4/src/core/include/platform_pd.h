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
			Sel_alloc() { _reserve(0, INITIAL_SEL_END); }
		};

	private:

		unsigned const _id;  /* used as index in top-level CNode */

		Page_table_registry _page_table_registry;

		Cap_sel const _page_directory_sel;
		addr_t  const _page_directory;

		Vm_space _vm_space;

		Cnode _cspace_cnode_1st;

		Constructible<Cnode> _cspace_cnode_2nd[1UL << CSPACE_SIZE_LOG2_1ST];

		Native_capability _parent { };

		Sel_alloc _sel_alloc { };
		Mutex _sel_alloc_mutex { };

		addr_t _init_page_directory() const;
		void   _deinit_page_directory(addr_t) const;

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
		 * Allocate capability selector
		 */
		Cap_sel alloc_sel();

		/**
		 * Release capability selector
		 */
		void free_sel(Cap_sel);

		/**
		 * Map physical IPC buffer to virtual UTCB address
		 */
		void map_ipc_buffer(Ipc_buffer_phys, Utcb_virt);

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

		Cnode &cspace_cnode(Cap_sel sel)
		{
			const unsigned index = sel.value() / (1 << CSPACE_SIZE_LOG2_2ND);
			ASSERT(index < sizeof(_cspace_cnode_2nd) /
	                       sizeof(_cspace_cnode_2nd[0]));

			return *_cspace_cnode_2nd[index];
		}

		Cnode &cspace_cnode_1st() { return _cspace_cnode_1st; }

		Cap_sel page_directory_sel() const { return _page_directory_sel; }

		size_t cspace_size_log2() { return CSPACE_SIZE_LOG2; }

		bool install_mapping(Mapping const &mapping, const char * thread_name);

		static Bit_allocator<1024> &pd_id_alloc();
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */
