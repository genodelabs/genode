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

using namespace Genode;
using Hw::Page_table;


/**************************************
 ** Hw::Address_space implementation **
 **************************************/

Core_mem_allocator &Hw::Address_space::_cma() {
	return static_cast<Core_mem_allocator &>(platform().core_mem_alloc()); }


void *Hw::Address_space::_table_alloc()
{
	void * ret = nullptr;
	if (!_cma().alloc_aligned(sizeof(Page_table), (void**)&ret,
	                          Page_table::ALIGNM_LOG2).ok())
		throw Insufficient_ram_quota();
	return ret;
}


bool Hw::Address_space::insert_translation(addr_t virt, addr_t phys,
                                           size_t size, Page_flags flags)
{
	try {
		for (;;) {
			try {
				Mutex::Guard guard(_mutex);
				_tt.insert_translation(virt, phys, size, flags, _tt_alloc);
				return true;
			} catch(Hw::Out_of_tables &) {
				flush(platform().vm_start(), platform().vm_size());
			}
		}
	} catch(...) {
		error("invalid mapping ", Hex(phys), " -> ", Hex(virt), " (", size, ")");
	}
	return false;
}


bool Hw::Address_space::lookup_translation(addr_t const virt, addr_t & phys)
{
	/** FIXME: for the time-being we use it without lock,
	 * because it is used directly by the kernel when cache_coherent_region
	 * gets called. In future it would be better that core provides an API
	 * for it, and does the lookup with the hold lock
	 */
	return _tt.lookup_translation(virt, phys, _tt_alloc);
}


void Hw::Address_space::flush(addr_t virt, size_t size, Core_local_addr)
{
	Mutex::Guard guard(_mutex);

	try {
		_tt.remove_translation(virt, size, _tt_alloc);
		Kernel::invalidate_tlb(*_kobj, virt, size);
	} catch(...) {
		error("tried to remove invalid region!");
	}
}


Hw::Address_space::Address_space(Page_table            & tt,
                                 Page_table::Allocator & tt_alloc,
                                 Platform_pd           & pd)
: _tt(tt),
  _tt_phys(Platform::core_page_table()),
  _tt_alloc(tt_alloc),
  _kobj(false, *(Page_table*)translation_table_phys(), pd) {}


Hw::Address_space::Address_space(Platform_pd & pd)
: _tt(*construct_at<Page_table>(_table_alloc(), *((Page_table*)Hw::Mm::core_page_tables().base))),
  _tt_phys((addr_t)_cma().phys_addr(&_tt)),
  _tt_array(new (_cma()) Array([] (void * virt) {
    return (addr_t)_cma().phys_addr(virt);})),
  _tt_alloc(_tt_array->alloc()),
  _kobj(true, *(Page_table*)translation_table_phys(), pd) { }


Hw::Address_space::~Address_space()
{
	flush(platform().vm_start(), platform().vm_size());
	destroy(_cma(), _tt_array);
	destroy(_cma(), &_tt);
}


/******************************
 ** Cap_space implementation **
 ******************************/

Cap_space::Cap_space() : _slab(nullptr, &_initial_sb) { }


void Cap_space::upgrade_slab(Allocator &alloc)
{
	void * block = nullptr;
	if (!alloc.alloc(SLAB_SIZE, &block))
		throw Out_of_ram();
	_slab.insert_sb(block);
}


/********************************
 ** Platform_pd implementation **
 ********************************/

bool Platform_pd::bind_thread(Platform_thread &t)
{
	/* is this the first and therefore main thread in this PD? */
	bool main_thread = !_thread_associated;
	_thread_associated = true;
	t.join_pd(this, main_thread, Address_space::weak_ptr());
	return true;
}


void Platform_pd::assign_parent(Native_capability parent)
{
	if (!_parent.valid() && parent.valid())
		_parent = parent;
}


Platform_pd::Platform_pd(Page_table & tt,
                         Page_table::Allocator & alloc)
: Hw::Address_space(tt, alloc, *this), _label("core") { }


Platform_pd::Platform_pd(Allocator &, char const *label)
: Hw::Address_space(*this), _label(label)
{
	if (!_kobj.cap().valid()) {
		error("failed to create kernel object");
		throw Service_denied();
	}
}

Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Address_space::lock_for_destruction();
}


/*************************************
 ** Core_platform_pd implementation **
 *************************************/

Core_platform_pd::Core_platform_pd()
: Platform_pd(*(Hw::Page_table*)Hw::Mm::core_page_tables().base,
              Platform::core_page_table_allocator()) { }
