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

namespace Genode { class Platform_pd; }


class Genode::Platform_pd : public Address_space
{
	private:

		unsigned const _id;  /* used as index in top-level CNode */

		Page_table_registry _page_table_registry;

		Cap_sel const _page_directory_sel;
		addr_t  const _page_directory;

		Vm_space _vm_space;

		Cnode _cspace_cnode_1st;

		Constructible<Cnode> _cspace_cnode_2nd[1UL << CSPACE_SIZE_LOG2_1ST];

		Native_capability _parent { };

		/*
		 * Allocator for core-managed selectors within the PD's CSpace
		 */
		typedef Bit_allocator<1 << NUM_CORE_MANAGED_SEL_LOG2> Sel_bit_alloc;

		struct Sel_alloc : Sel_bit_alloc
		{
			Sel_alloc() { _reserve(0, INITIAL_SEL_END); }
		};

		Sel_alloc _sel_alloc { };
		Mutex _sel_alloc_mutex { };

		Cap_sel alloc_sel();
		void free_sel(Cap_sel sel);

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
		 * Bind thread to protection domain
		 */
		bool bind_thread(Platform_thread &);

		/**
		 * Unbind thread from protection domain
		 *
		 * Free the thread's slot and update thread object.
		 */
		void unbind_thread(Platform_thread &);

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
