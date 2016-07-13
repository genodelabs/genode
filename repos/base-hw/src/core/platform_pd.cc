/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \author  Sebastian Sumpf
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <root/root.h>

/* core includes */
#include <platform_pd.h>
#include <platform_thread.h>

extern int _prog_img_beg;
extern int _prog_img_end;

using namespace Genode;


/**************************************
 ** Hw::Address_space implementation **
 **************************************/

Core_mem_allocator * Hw::Address_space::_cma() {
	return static_cast<Core_mem_allocator*>(platform()->core_mem_alloc()); }


void * Hw::Address_space::_table_alloc()
{
	void * ret;
	if (!_cma()->alloc_aligned(sizeof(Translation_table), (void**)&ret,
	                           Translation_table::ALIGNM_LOG2).ok())
		throw Root::Quota_exceeded();
	return ret;
}


bool Hw::Address_space::insert_translation(addr_t virt, addr_t phys,
                                           size_t size, Page_flags flags)
{
	try {
		for (;;) {
			try {
				Lock::Guard guard(_lock);
				_tt->insert_translation(virt, phys, size, flags, _tt_alloc);
				return true;
			} catch(Allocator::Out_of_memory) {
				flush(platform()->vm_start(), platform()->vm_size());
			}
		}
	} catch(...) {
		error("invalid mapping ", Hex(phys), " -> ", Hex(virt), " (", size, ")");
	}
	return false;
}


void Hw::Address_space::flush(addr_t virt, size_t size)
{
	Lock::Guard guard(_lock);

	try {
		if (_tt) _tt->remove_translation(virt, size, _tt_alloc);

		/* update translation caches */
		Kernel::update_pd(_kernel_pd);
	} catch(...) {
		error("tried to remove invalid region!");
	}
}


Hw::Address_space::Address_space(Kernel::Pd* pd, Translation_table * tt,
                                 Translation_table_allocator * tt_alloc)
: _tt(tt), _tt_phys(tt), _tt_alloc(tt_alloc), _kernel_pd(pd) { }


Hw::Address_space::Address_space(Kernel::Pd * pd)
: _tt(construct_at<Translation_table>(_table_alloc())),
  _tt_phys(reinterpret_cast<Translation_table*>(_cma()->phys_addr(_tt))),
  _tt_alloc((new (_cma()) Table_allocator(_cma()))->alloc()),
  _kernel_pd(pd)
{
	Lock::Guard guard(_lock);
	Kernel::mtc()->map(_tt, _tt_alloc);
}


Hw::Address_space::~Address_space()
{
	flush(platform()->vm_start(), platform()->vm_size());
	destroy(_cma(), Table_allocator::base(_tt_alloc));
	destroy(_cma(), _tt);
}


/******************************
 ** Cap_space implementation **
 ******************************/

Cap_space::Cap_space() : _slab(nullptr, &_initial_sb) { }


void Cap_space::upgrade_slab(Allocator &alloc)
{
	for (;;) {
		void *block = nullptr;

		/*
		 * On every upgrade we try allocating as many blocks as possible.
		 * If the underlying allocator complains that its quota is exceeded
		 * this is normal as we use it as indication when to exit the loop.
		 */
		if (!alloc.alloc(SLAB_SIZE, &block)) return;
		_slab.insert_sb(block);
	}
}


/********************************
 ** Platform_pd implementation **
 ********************************/

bool Platform_pd::bind_thread(Platform_thread * t)
{
	/* is this the first and therefore main thread in this PD? */
	bool main_thread = !_thread_associated;
	_thread_associated = true;
	t->join_pd(this, main_thread, Address_space::weak_ptr());
	return true;
}


void Platform_pd::unbind_thread(Platform_thread *t) {
	t->join_pd(nullptr, false, Address_space::weak_ptr()); }


void Platform_pd::assign_parent(Native_capability parent)
{
	if (!_parent.valid() && parent.valid())
		_parent = parent;
}


Platform_pd::Platform_pd(Translation_table * tt,
                         Translation_table_allocator * alloc)
: Hw::Address_space(kernel_object(), tt, alloc),
  Kernel_object<Kernel::Pd>(false, tt, this),
  _label("core") { }


Platform_pd::Platform_pd(Allocator * md_alloc, char const *label)
: Hw::Address_space(kernel_object()),
  Kernel_object<Kernel::Pd>(true, translation_table_phys(), this),
  _label(label)
{
	if (!_cap.valid()) {
		error("failed to create kernel object");
		throw Root::Unavailable();
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

Translation_table * const Core_platform_pd::_table()
{
	return unmanaged_singleton<Translation_table,
	                           1 << Translation_table::ALIGNM_LOG2>();
}


Translation_table_allocator * const Core_platform_pd::_table_alloc()
{
	constexpr size_t count = Genode::Translation_table::CORE_TRANS_TABLE_COUNT;
	using Allocator = Translation_table_allocator_tpl<count>;

	static Allocator * alloc = nullptr;
	if (!alloc) {
		void * base = (void*) Platform::core_translation_tables();
		alloc = construct_at<Allocator>(base);
	}
	return alloc->alloc();
}


void Core_platform_pd::_map(addr_t start, addr_t end, bool io_mem)
{
	const Page_flags flags =
		Page_flags::apply_mapping(true, io_mem ? UNCACHED : CACHED, io_mem);

	start = trunc_page(start);

	/* omitt regions before vm_start */
	if (start < VIRT_ADDR_SPACE_START)
		start = VIRT_ADDR_SPACE_START;

	size_t size  = round_page(end) - start;
	try {
		_table()->insert_translation(start, start, size, flags, _table_alloc());
	} catch(Allocator::Out_of_memory) {
		error("translation table needs to much RAM");
	} catch(...) {
		error("invalid mapping ", Hex(start), " size=", size);
	}
}


Core_platform_pd::Core_platform_pd()
: Platform_pd(_table(), _table_alloc())
{
	/* map exception vector for core */
	Kernel::mtc()->map(_table(), _table_alloc());

	/* map core's program image */
	_map((addr_t)&_prog_img_beg, (addr_t)&_prog_img_end, false);

	/* map core's page table allocator */
	_map(Platform::core_translation_tables(),
	     Platform::core_translation_tables() +
	     Platform::core_translation_tables_size(), false);

	/* map core's mmio regions */
	Native_region * r = Platform::_core_only_mmio_regions(0);
	for (unsigned i = 0; r;
		 r = Platform::_core_only_mmio_regions(++i))
		_map(r->base, r->base + r->size, true);
}
