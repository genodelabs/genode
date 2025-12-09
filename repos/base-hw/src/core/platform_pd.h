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
#include <page_table_allocator.h>
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <kernel/pd.h>
#include <hw_native_pd/hw_native_pd.h>

namespace Core {

	class Platform_thread; /* forward declaration */

	class Cap_space;
	class Platform_pd_interface;

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


class Core::Cap_space
{
	protected:

		uint8_t _initial_sb[Kernel::CAP_SLAB_SIZE];
		Kernel::Cap_slab _slab;

	public:

		using Upgrade_result = Genode::Pd_session::Native_pd::Upgrade_result;

		Cap_space();

		Upgrade_result upgrade_slab(Allocator &alloc);
		size_t avail_slab() { return _slab.avail_entries(); }
};


struct Core::Platform_pd_interface : Genode::Interface
{
	using Name = Kernel::Pd::Core_pd_data::Name;

	virtual Native_capability parent() const = 0;

	virtual Name name() const = 0;

	virtual Kernel::Pd & kernel_pd() = 0;
};


class Core::Platform_pd
: public Address_space, public Platform_pd_interface, private Cap_space
{
	private:

		using Id_alloc_result = Attempt<addr_t, Bit_array_base::Error>;
		using Page_table_array = Hw::Page_table_array<4096, 16>;

		Name const _name;

		Native_capability _parent {};

		Genode::Mutex _mutex { }; /* table lock */

		Phys_allocated<Hw::Page_table>   _table;
		Page_table_allocator             _table_alloc;

		Id_alloc_result _id;

		Kernel_object<Kernel::Pd> _kobj;

		Kernel::Pd::Core_pd_data _core_pd_data();

		bool _thread_associated = false;

		bool _warned_once_about_quota_depletion = false;

		friend class Platform_thread;

	public:

		using Constructed = Attempt<Ok, Accounted_mapped_ram_allocator::Error>;
		Constructed const constructed =
			_id.failed() ? Accounted_mapped_ram_allocator::Error::DENIED
			             : _table.constructed;

		/*
		 * Noncopyable
		 */
		Platform_pd(Platform_pd const &) = delete;
		Platform_pd &operator = (Platform_pd const &) = delete;

		Platform_pd(Accounted_mapped_ram_allocator &, Allocator &,
		            Name const &);

		~Platform_pd();

		bool map(addr_t virt, addr_t phys,
		         size_t size, Page_flags flags);

		void flush(addr_t virt, size_t size, Core_local_addr) override;
		void flush_all();

		Native_capability parent() const override { return _parent; }

		Name name() const override { return _name; }

		Kernel::Pd & kernel_pd() override { return *_kobj; }

		void assign_parent(Native_capability parent) {
			if (!_parent.valid() && parent.valid()) _parent = parent; }

		using Cap_space::upgrade_slab;
		using Cap_space::avail_slab;
};


class Core::Core_platform_pd : public Platform_pd_interface, private Cap_space
{
	private:

		Genode::Mutex             _mutex { };   /* table lock      */
		Hw::Page_table           &_table;       /* table virt addr */
		Hw::Page_table_allocator &_table_alloc; /* table allocator */
		Kernel_object<Kernel::Pd> _kobj;

		friend bool map_local(addr_t, addr_t, size_t, Page_flags);
		friend bool unmap_local(addr_t, size_t);

	public:

		Core_platform_pd();

		using Cap_space::upgrade_slab;
		using Cap_space::avail_slab;

		Native_capability parent() const override {
			return Native_capability(); }

		Name name() const override { return { "core" }; }

		Kernel::Pd & kernel_pd() override { return *_kobj; }
};

#endif /* _CORE__PLATFORM_PD_H_ */
