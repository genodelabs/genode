/*
 * \brief   Platform specific part of a Genode protection domain
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__PLATFORM_PD_H_
#define _CORE__PLATFORM_PD_H_

/* core includes */
#include <platform.h>
#include <address_space.h>
#include <object.h>
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <kernel/pd.h>

/* base-hw internal includes */
#include <hw/page_table_allocator.h>

namespace Core {

	/**
	 * Memory virtualization interface of a protection domain
	 */
	class Hw_address_space;

	class Platform_thread; /* forward declaration */

	class Cap_space;

	/**
	 * Platform specific part of a protection domain
	 */
	class Platform_pd;

	/**
	 * Platform specific part of core's protection domain
	 */
	class Core_platform_pd;

	using Hw::Page_flags;
}


class Core::Hw_address_space : public Core::Address_space
{
	public:

		using Id_allocator = Board::Address_space_id_allocator;
		using Table = Hw::Page_table;
		using Table_allocator = Hw::Page_table_allocator;

		/*
		 * Noncopyable
		 */
		Hw_address_space(Hw_address_space const &) = delete;
		Hw_address_space &operator = (Hw_address_space const &) = delete;

	private:

		friend class Core::Platform;
		friend class Core::Mapped_mem_allocator;

		Genode::Mutex    _mutex { };             /* table lock      */
		Table           &_table;                 /* table virt addr */
		addr_t           _table_phys;            /* table phys addr */
		Table::Array    *_table_array = nullptr;
		Table_allocator &_table_alloc;           /* table allocator */

		static inline Core_mem_allocator &_cma();

		static inline void *_alloc_table();

	protected:

		Kernel_object<Kernel::Pd> _kobj;

		Kernel::Pd::Core_pd_data _core_pd_data(Platform_pd &);

		/**
		 * Constructor used for the Core PD object
		 *
		 * \param tt        reference to translation table
		 * \param tt_alloc  reference to translation table allocator
		 * \param pd        reference to platform pd object
		 */
		Hw_address_space(Table           &tt,
		                 Table_allocator &tt_alloc,
		                 Platform_pd     &pd,
		                 Id_allocator    &addr_space_id_alloc);

	public:

		/**
		 * Constructor used for objects other than the Core PD
		 *
		 * \param pd    reference to platform pd object
		 */
		Hw_address_space(Platform_pd &pd);

		~Hw_address_space();

		/**
		 * Insert memory mapping into translation table of the address space
		 *
		 * \param virt   virtual start address
		 * \param phys   physical start address
		 * \param size   size of memory region
		 * \param flags  translation table flags (e.g. caching attributes)
		 */
		bool insert_translation(addr_t virt, addr_t phys,
		                        size_t size, Page_flags flags);

		bool lookup_rw_translation(addr_t const virt, addr_t &phys);


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t, Core_local_addr) override;
		void flush(addr_t addr, size_t size) {
			flush(addr, size, Core_local_addr{0}); }


		/***************
		 ** Accessors **
		 ***************/

		Kernel::Pd &kernel_pd()         { return *_kobj;      }
		Table &translation_table()      { return _table;      }
		addr_t translation_table_phys() { return _table_phys; }
};


class Core::Cap_space
{
	protected:

		uint8_t _initial_sb[Kernel::CAP_SLAB_SIZE];
		Kernel::Cap_slab _slab;

	public:

		Cap_space();

		void upgrade_slab(Allocator &alloc);
		size_t avail_slab() { return _slab.avail_entries(); }
};


class Core::Platform_pd : public Hw_address_space, private Cap_space
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_pd(Platform_pd const &);
		Platform_pd &operator = (Platform_pd const &);

		Native_capability  _parent { };
		bool               _thread_associated = false;
		char const * const _label;

		friend class Hw_address_space;

	protected:

		/**
		 * Constructor used for the Core PD object
		 *
		 * \param tt        translation table address
		 * \param tt_alloc  translation table allocator
		 */
		Platform_pd(Table           &tt,
		            Table_allocator &tt_alloc,
		            Id_allocator    &addr_space_id_alloc);

	public:

		bool has_any_thread = false;

		/**
		 * Constructor used for objects other than the Core PD
		 *
		 * \param label  name of protection domain
		 */
		Platform_pd(Allocator &md_alloc, char const *label);

		/**
		 * Destructor
		 */
		~Platform_pd();

		using Cap_space::upgrade_slab;
		using Cap_space::avail_slab;

		/**
		 * Bind thread to protection domain
		 */
		bool bind_thread(Platform_thread &);

		/**
		 * Assign parent interface to protection domain
		 */
		void assign_parent(Native_capability parent);


		/***************
		 ** Accessors **
		 ***************/

		char const *      label()  { return _label;  }
		Native_capability parent() { return _parent; }
};


struct Core::Core_platform_pd : Platform_pd
{
	Core_platform_pd(Id_allocator &id_alloc);
};

#endif /* _CORE__PLATFORM_PD_H_ */
