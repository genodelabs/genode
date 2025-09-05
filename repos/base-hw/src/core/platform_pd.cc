/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \author  Sebastian Sumpf
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <root/root.h>

/* core includes */
#include <hw/assert.h>
#include <platform_pd.h>
#include <platform_thread.h>

using namespace Core;
using Hw::Page_table;
using Hw::Page_table_allocator;


/*************************************
 ** Hw_address_space implementation **
 *************************************/

Core_mem_allocator &Hw_address_space::_cma()
{
	return static_cast<Core_mem_allocator &>(platform().core_mem_alloc());
}


void *Hw_address_space::_alloc_table()
{
	unsigned const align = Page_table::ALIGNM_LOG2;

	return _cma().alloc_aligned(sizeof(Page_table), align).convert<void *>(

		[&] (Range_allocator::Allocation &a) {
			a.deallocate = false; return a.ptr; },

		[&] (Range_allocator::Alloc_result) -> void * {
			/* XXX distinguish error conditions */
			throw Insufficient_ram_quota(); });
}


bool Hw_address_space::insert_translation(addr_t virt, addr_t phys,
                                          size_t size, Page_flags flags)
{
	using Result = Hw::Page_table::Result;

	Result result = Ok();

	for (;;) {
		bool retry = false;

		{
			Mutex::Guard guard(_mutex);
			result = _table.insert(virt, phys, size, flags, _table_alloc);
		}

		result.with_error([&] (Hw::Page_table_error e) {
			if (e == Hw::Page_table_error::INVALID_RANGE)
				return;

			/* core/kernel's page-tables should never get flushed */
			if (_table_phys == Platform::core_page_table()) {
				error("core's page-table allocator is empty!");
				return;
			}

			flush(platform().vm_start(), platform().vm_size());
			retry = true;
		});
		if (!retry)
			break;
	}

	return result.convert<bool>([&] (Ok) -> bool { return true; },
	                            [&] (Hw::Page_table_error) -> bool {
		error("invalid mapping ", Hex(phys), " -> ", Hex(virt), " (", size, ")");
		return false; });
}


bool Hw_address_space::lookup_rw_translation(addr_t const virt, addr_t &phys)
{
	/** FIXME: for the time-being we use it without lock,
	 * because it is used directly by the kernel when cache_coherent_region
	 * gets called. In future it would be better that core provides an API
	 * for it, and does the lookup with the hold lock
	 */
	return _table.lookup(virt, phys, _table_alloc).convert<bool>(
		[] (Ok) -> bool { return true; },
		[] (Hw::Page_table_error) -> bool { return false; });
}


void Hw_address_space::flush(addr_t virt, size_t size, Core_local_addr)
{
	Mutex::Guard guard(_mutex);

	_table.remove(virt, size, _table_alloc);
	Kernel::invalidate_tlb(*_kobj, virt, size);
}


Kernel::Pd::Core_pd_data Hw_address_space::_core_pd_data(Platform_pd &pd)
{
	return Kernel::Pd::Core_pd_data { _table_phys, &_table,
	                                  _table_alloc, pd._slab, pd.name };
}


Hw_address_space::
Hw_address_space(Page_table                        &table,
                 Page_table_allocator              &table_alloc,
                 Platform_pd                       &pd,
                 Board::Address_space_id_allocator &addr_space_id_alloc)
:
	_table(table),
	_table_phys(Platform::core_page_table()),
	_table_alloc(table_alloc),
	_kobj(_kobj.CALLED_FROM_KERNEL,
	      _core_pd_data(pd), addr_space_id_alloc)
{ }


Hw_address_space::Hw_address_space(Platform_pd &pd)
:
	_table(*construct_at<Page_table>(_alloc_table(),
	                                 *((Page_table*)Hw::Mm::core_page_tables().base))),
	_table_phys((addr_t)_cma().phys_addr(&_table)),
	_table_array(new (_cma()) Table::Array([] (void * virt) {
	                             return (addr_t)_cma().phys_addr(virt);})),
	_table_alloc(_table_array->alloc()),
	_kobj(_kobj.CALLED_FROM_CORE, _core_pd_data(pd))
{ }


Hw_address_space::~Hw_address_space()
{
	flush(platform().vm_start(), platform().vm_size());
	destroy(_cma(), _table_array);
	destroy(_cma(), &_table);
}


/******************************
 ** Cap_space implementation **
 ******************************/

Cap_space::Cap_space() : _slab(nullptr, &_initial_sb) { }


void Cap_space::upgrade_slab(Allocator &alloc)
{
	alloc.try_alloc(Kernel::CAP_SLAB_SIZE).with_result(
		[&] (Memory::Allocation &a) {
			a.deallocate = false;
			_slab.insert_sb(a.ptr);
		},
		[&] (Alloc_error e) { raise(e); });
}


/********************************
 ** Platform_pd implementation **
 ********************************/

void Platform_pd::assign_parent(Native_capability parent)
{
	if (!_parent.valid() && parent.valid())
		_parent = parent;
}


Platform_pd::
Platform_pd(Page_table                        &tt,
            Page_table_allocator              &alloc,
            Board::Address_space_id_allocator &addr_space_id_alloc)
:
	Hw_address_space(tt, alloc, *this, addr_space_id_alloc), name("core")
{ }


Platform_pd::Platform_pd(Allocator &, Name const &name)
:
	Hw_address_space(*this), name(name)
{
	if (!_kobj.cap().valid()) {
		error("failed to create kernel object");
		throw Service_denied();
	}
}


Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Hw_address_space::lock_for_destruction();
}


/*************************************
 ** Core_platform_pd implementation **
 *************************************/

Core_platform_pd::
Core_platform_pd(Board::Address_space_id_allocator &addr_space_id_alloc)
:
	Platform_pd(*(Hw::Page_table*)Hw::Mm::core_page_tables().base,
	            Platform::core_page_table_allocator(), addr_space_id_alloc)
{ }
