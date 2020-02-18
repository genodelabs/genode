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

/* Core includes */
#include <translation_table.h>
#include <platform.h>
#include <address_space.h>
#include <hw/page_table_allocator.h>
#include <object.h>
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <kernel/pd.h>

namespace Hw
{
	using namespace Kernel;
	using namespace Genode;

	/**
	 * Memory virtualization interface of a protection domain
	 */
	class Address_space;
}

namespace Genode
{
	class Platform_thread; /* forward declaration */

	class Cap_space;

	/**
	 * Platform specific part of a Genode protection domain
	 */
	class Platform_pd;

	/**
	 * Platform specific part of Core's protection domain
	 */
	class Core_platform_pd;

	using Hw::Page_flags;
}


class Hw::Address_space : public Genode::Address_space
{
	private:

		/*
		 * Noncopyable
		 */
		Address_space(Address_space const &);
		Address_space &operator = (Address_space const &);

		friend class Genode::Platform;
		friend class Genode::Mapped_mem_allocator;

		using Table = Hw::Page_table;
		using Array = Table::Allocator::Array<DEFAULT_TRANSLATION_TABLE_MAX>;

		Genode::Mutex               _mutex { };          /* table lock      */
		Table                     & _tt;                 /* table virt addr */
		Genode::addr_t              _tt_phys;            /* table phys addr */
		Array                     * _tt_array = nullptr;
		Table::Allocator          & _tt_alloc;           /* table allocator */

		static inline Genode::Core_mem_allocator &_cma();

		static inline void *_table_alloc();

	protected:

		Kernel_object<Kernel::Pd> _kobj;

		/**
		 * Core-specific constructor
		 *
		 * \param tt        reference to translation table
		 * \param tt_alloc  reference to translation table allocator
		 * \param pd        reference to platform pd object
		 */
		Address_space(Hw::Page_table            & tt,
		              Hw::Page_table::Allocator & tt_alloc,
		              Platform_pd               & pd);

	public:

		/**
		 * Constructor
		 *
		 * \param pd    reference to platform pd object
		 */
		Address_space(Platform_pd & pd);

		~Address_space();

		/**
		 * Insert memory mapping into translation table of the address space
		 *
		 * \param virt   virtual start address
		 * \param phys   physical start address
		 * \param size   size of memory region
		 * \param flags  translation table flags (e.g. caching attributes)
		 */
		bool insert_translation(Genode::addr_t virt, Genode::addr_t phys,
		                        Genode::size_t size, Genode::Page_flags flags);

		bool lookup_translation(Genode::addr_t const virt,
		                        Genode::addr_t & phys);

		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(addr_t, size_t, Core_local_addr) override;
		void flush(addr_t addr, size_t size) {
			flush(addr, size, Core_local_addr{0}); }


		/***************
		 ** Accessors **
		 ***************/

		Kernel::Pd     & kernel_pd()              { return *_kobj;   }
		Hw::Page_table & translation_table()      { return _tt;      }
		Genode::addr_t   translation_table_phys() { return _tt_phys; }
};


class Genode::Cap_space
{
	private:

		enum { SLAB_SIZE = 2 * get_page_size() };

		using Cap_slab = Tslab<Kernel::Object_identity_reference,
		                       SLAB_SIZE>;

		uint8_t    _initial_sb[SLAB_SIZE];
		Cap_slab   _slab;

	public:

		Cap_space();

		Cap_slab & capability_slab() { return _slab; }

		void upgrade_slab(Allocator &alloc);
};


class Genode::Platform_pd : public  Hw::Address_space,
                            private Cap_space
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

	protected:

		/**
		 * Constructor for core pd
		 *
		 * \param tt        translation table address
		 * \param tt_alloc  translation table allocator
		 */
		Platform_pd(Hw::Page_table            & tt,
		            Hw::Page_table::Allocator & tt_alloc);

	public:

		/**
		 * Constructor for non-core pd
		 *
		 * \param label  name of protection domain
		 */
		Platform_pd(Allocator &md_alloc, char const *label);

		/**
		 * Destructor
		 */
		~Platform_pd();

		using Cap_space::capability_slab;
		using Cap_space::upgrade_slab;

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


struct Genode::Core_platform_pd : Genode::Platform_pd
{
	Core_platform_pd();
};

#endif /* _CORE__PLATFORM_PD_H_ */
