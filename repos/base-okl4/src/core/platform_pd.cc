/*
 * \brief   OKL4-specific protection-domain facility
 * \author  Norman Feske
 * \date    2009-03-31
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/lock_guard.h>

/* core includes */
#include <util.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <platform.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/config.h>
#include <l4/ipc.h>
} }

using namespace Genode;


/****************************
 ** Private object members **
 ****************************/

void Platform_pd::_create_pd(bool syscall)
{
	using namespace Okl4;

	if (!syscall)
		return;

	L4_Word_t    control        = L4_SpaceCtrl_new;
	L4_ClistId_t cap_list       = L4_rootclist;
	L4_Word_t    utcb_area_size = L4_GetUtcbSize()*(1 << Thread_id_bits::THREAD);
	L4_Word_t    utcb_location  = platform_specific()->utcb_base()
	                            + _pd_id*utcb_area_size;
	L4_Fpage_t   utcb_area      = L4_Fpage(utcb_location, utcb_area_size);
	L4_Word_t    resources      = 0;
	L4_Word_t    old_resources  = 0;

#ifdef NO_UTCB_RELOCATE
	utcb_area = L4_Nilpage;  /* UTCB allocation is handled by the kernel */
#endif

	/* create address space */
	int ret = L4_SpaceControl(L4_SpaceId(_pd_id), control, cap_list, utcb_area,
	                          resources, &old_resources);

	if (ret != 1)
		error("L4_SpaceControl(new) returned ", ret, ", error=", L4_ErrorCode());
}


void Platform_pd::_destroy_pd()
{
	using namespace Okl4;

	L4_Word_t    control        = L4_SpaceCtrl_delete;
	L4_ClistId_t cap_list       = L4_rootclist;
	L4_Word_t    utcb_area_size = L4_GetUtcbSize()*(1 << Thread_id_bits::THREAD);
	L4_Word_t    utcb_location  = platform_specific()->utcb_base()
	                            + _pd_id*utcb_area_size;
	L4_Fpage_t   utcb_area      = L4_Fpage(utcb_location, utcb_area_size);
	L4_Word_t    resources      = 0;
	L4_Word_t    old_resources  = 0;

#ifdef NO_UTCB_RELOCATE
	utcb_area = L4_Nilpage;  /* UTCB allocation is handled by the kernel */
#endif

	int ret = L4_SpaceControl(L4_SpaceId(_pd_id), control, cap_list, utcb_area,
	                          resources, &old_resources);

	if (ret != 1)
		error("L4_SpaceControl(delete) returned ", ret, ", error=", L4_ErrorCode());
}


int Platform_pd::_alloc_pd()
{
	unsigned i;

	for (i = PD_FIRST; i <= PD_MAX; i++)
		if (_pds()[i].free) break;

	/* no free protection domains available */
	if (i > PD_MAX) return -1;

	_pds()[i].free = 0;

	return i;
}


void Platform_pd::_free_pd()
{
	unsigned id = _pd_id;

	if (_pds()[id].free) return;

	_pds()[id].free = 1;
}


void Platform_pd::_init_threads()
{
	unsigned i;

	for (i = 0; i < THREAD_MAX; ++i)
		_threads[i] = 0;
}


Platform_thread* Platform_pd::_next_thread()
{
	unsigned i;

	/* look for bound thread */
	for (i = 0; i < THREAD_MAX; ++i)
		if (_threads[i]) break;

	/* no bound threads */
	if (i == THREAD_MAX) return 0;

	return _threads[i];
}


int Platform_pd::_alloc_thread(int thread_id, Platform_thread *thread)
{
	int i = thread_id;

	/* look for free thread */
	if (thread_id == Platform_thread::THREAD_INVALID) {
		for (i = 0; i < THREAD_MAX; ++i)
			if (!_threads[i]) break;

		/* no free threads available */
		if (i == THREAD_MAX) return -1;
	} else {
		if (_threads[i]) return -2;
	}

	_threads[i] = thread;

	return i;
}


void Platform_pd::_free_thread(int thread_id)
{
	if (!_threads[thread_id])
		warning("double-free of thread ",
		        Hex(_pd_id), ".", Hex(thread_id), " detected");

	_threads[thread_id] = 0;
}


/***************************
 ** Public object members **
 ***************************/

bool Platform_pd::bind_thread(Platform_thread *thread)
{
	using namespace Okl4;

	/* thread_id is THREAD_INVALID by default - only core is the special case */
	int thread_id = thread->thread_id();
	L4_ThreadId_t l4_thread_id;

	int t = _alloc_thread(thread_id, thread);
	if (t < 0) {
		error("thread alloc failed");
		return false;
	}
	thread_id = t;
	l4_thread_id = make_l4_id(_pd_id, thread_id);

	/* finally inform thread about binding */
	thread->bind(thread_id, l4_thread_id, this);
	return true;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	int thread_id = thread->thread_id();

	/* unbind thread before proceeding */
	thread->unbind();

	_free_thread(thread_id);
}


void Platform_pd::space_pager(Platform_thread *thread)
{
	using namespace Okl4;

	L4_Word_t control           = L4_SpaceCtrl_space_pager;
	L4_SpaceId_t pager_space    = L4_SpaceId(thread->pd()->pd_id());
	L4_ClistId_t cap_list       = L4_rootclist;
	L4_Word_t    utcb_area_size = L4_GetUtcbSize()*(1 << Thread_id_bits::THREAD);
	L4_Word_t    utcb_location  = platform_specific()->utcb_base()
	                            + _pd_id*utcb_area_size;
	L4_Fpage_t   utcb_area      = L4_Fpage(utcb_location, utcb_area_size);
	L4_Word_t    resources      = 0;
	L4_Word_t    old_resources  = 0;

	/* set the space pager */
	_space_pager = thread;
	L4_LoadMR(0,pager_space.raw);
	int ret = L4_SpaceControl(L4_SpaceId(_pd_id), control, cap_list, utcb_area,
	                      resources, &old_resources);

	if (ret != 1)
		error("L4_SpaceControl(new space_pager...) returned ", ret, ", error=",
		      L4_ErrorCode());

	/* grant the pager mapping rights regarding this space */
	if(!L4_AllowUserMapping(pager_space, 0x0, 0xff000000))
		error("failed to delegate pt access to ", Hex(pager_space.raw), ", "
		      "error=", L4_ErrorCode());
}


void Platform_pd::_setup_address_space() { }


static void unmap_log2_range(unsigned pd_id, addr_t base, size_t size_log2)
{
	using namespace Okl4;

	L4_Fpage_t fpage = L4_FpageLog2(base, size_log2);
	L4_FpageAddRightsTo(&fpage, L4_FullyAccessible);
	int ret = L4_UnmapFpage(L4_SpaceId(pd_id), fpage);
	if (ret != 1)
		error("could not unmap page at ", Hex(base), " from space ", Hex(pd_id), ", "
		      "error=", L4_ErrorCode());
}


void Platform_pd::flush(addr_t addr, size_t size)
{
	using namespace Okl4;

	L4_Word_t remaining_size = size;
	L4_Word_t size_log2      = get_page_size_log2();

	/*
	 * Let unmap granularity ('size_log2') grow
	 */
	while (remaining_size >= (1UL << size_log2)) {

		enum { SIZE_LOG2_MAX = 22 /* 4M */ };

		/* issue 'unmap' for the current address if flexpage aligned */
		if (addr & (1 << size_log2)) {
			unmap_log2_range(_pd_id, addr, size_log2);

			remaining_size -= 1 << size_log2;
			addr           += 1 << size_log2;
		}

		/* increase flexpage size */
		size_log2++;
	}

	/*
	 * Let unmap granularity ('size_log2') shrink
	 */
	while (remaining_size > 0) {

		if (remaining_size >= (1UL << size_log2)) {
			unmap_log2_range(_pd_id, addr, size_log2);

			remaining_size -= 1 << size_log2;
			addr           += 1 << size_log2;
		}

		/* decrease flexpage size */
		size_log2--;
	}
}


Platform_pd::Platform_pd(bool core)
: _space_pager(0)
{
	/* init remainder */
	Pd_alloc free(false, true);
	for (unsigned i = 0 ; i <= PD_MAX; ++i) _pds()[i] = free;

	_init_threads();

	_pd_id = _alloc_pd();

	_create_pd(false);
}


Platform_pd::Platform_pd(Allocator *, char const *label)
: _space_pager(0)
{
	_init_threads();

	_pd_id = _alloc_pd();

	if (_pd_id > PD_MAX)
		error("pd alloc failed");

	_create_pd(true);
}


Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Address_space::lock_for_destruction();

	/* unbind all threads */
	while (Platform_thread *t = _next_thread()) unbind_thread(t);

	_destroy_pd();
	_free_pd();
}

