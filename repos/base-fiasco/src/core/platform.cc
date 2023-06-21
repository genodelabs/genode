/*
 * \brief   Fiasco platform interface implementation
 * \author  Christian Helmuth
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/sleep.h>
#include <util/misc_math.h>
#include <util/xml_generator.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/fiasco_thread_helper.h>
#include <base/internal/stack_area.h>
#include <base/internal/capability_space_tpl.h>
#include <base/internal/globals.h>

/* core includes */
#include <core_log.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

using namespace Core;
using namespace Fiasco;


/***********************************
 ** Core address space management **
 ***********************************/

static Synced_range_allocator<Allocator_avl> &_core_address_ranges()
{
	static Synced_range_allocator<Allocator_avl> _core_address_ranges(nullptr);
	return _core_address_ranges;
}


enum { PAGER_STACK_ELEMENTS = 1024 };
static unsigned long _core_pager_stack[PAGER_STACK_ELEMENTS];
static unsigned _core_pager_arg;


/**
 * Core pager "service loop"
 */
static void _core_pager_loop()
{
	unsigned pd_id = _core_pager_arg;

	l4_threadid_t t;
	l4_umword_t dw0, dw1;
	l4_msgdope_t r;

	bool send_reply = false;

	while (1) {

		if (send_reply)
			/* unblock faulter and wait for next pagefault */
			l4_ipc_reply_and_wait(t, L4_IPC_SHORT_MSG, 0, 0,
			                      &t, L4_IPC_SHORT_MSG, &dw0, &dw1,
			                      L4_IPC_NEVER, &r);
		else
			l4_ipc_wait(&t, L4_IPC_SHORT_MSG, &dw0, &dw1, L4_IPC_NEVER, &r);

		/* ignore messages from non-core pds */
		if (t.id.task != pd_id) break;

		/* detect local map request */
		if (dw1 == 0) {
			l4_msgdope_t ipc_result;
			l4_ipc_send(t, L4_IPC_SHORT_FPAGE, 0, dw0,
			            L4_IPC_SEND_TIMEOUT_0, &ipc_result);
			send_reply = false;
			continue;
		}

		bool rw = dw0 & 2;
		addr_t pfa = dw0 & ~2;

		if (pfa < L4_PAGESIZE) {

			/* NULL pointer access */
			error("possible null pointer ", rw ? "WRITE" : "READ", " "
			      "in ", (int)t.id.task, ".", (int)t.id.lthread, " "
			      "at ", Hex(pfa), " IP ", Hex(dw1));
			/* do not unblock faulter */
			send_reply = false;
			continue;
		} else if (!_core_address_ranges().valid_addr(pfa)) {

			/* page-fault address is not in RAM */
			error(rw ? "WRITE" : "READ", " access outside of RAM "
			      "in ", (int)t.id.task, ".", (int)t.id.lthread, " "
			      "at ", Hex(pfa), " IP ", Hex(dw1));
			/* do not unblock faulter */
			send_reply = false;
			continue;
		}

		/* my pf handler is sigma0 - just touch the appropriate page */
		if (rw)
			touch_rw((void *)pfa, 1);
		else
			touch_ro((void *)pfa, 1);

		send_reply = true;
	}
}


Core::Platform::Sigma0::Sigma0()
:
	Pager_object(Cpu_session_capability(), Thread_capability(),
	             0, Affinity::Location(), Session_label(),
	             Cpu_session::Name("sigma0"))
{
	cap(Capability_space::import(sigma0_threadid, Rpc_obj_key()));
}


Core::Platform::Sigma0 &Core::Platform::sigma0()
{
	static Sigma0 _sigma0;
	return _sigma0;
}


Core::Platform::Core_pager::Core_pager(Platform_pd &core_pd)
:
	Platform_thread("core.pager"),
	Pager_object(Cpu_session_capability(), Thread_capability(),
	             0, Affinity::Location(), Session_label(),
	             Cpu_session::Name(name()))
{
	Platform_thread::pager(sigma0());

	core_pd.bind_thread(*this);
	cap(Capability_space::import(native_thread_id(), Rpc_obj_key()));

	/* pager needs to know core's pd ID */
	_core_pager_arg = core_pd.pd_id();

	/* stack begins at the top end of the '_core_pager_stack' array */
	void *sp = (void *)&_core_pager_stack[PAGER_STACK_ELEMENTS - 1];
	start((void *)_core_pager_loop, sp);

	/* pager0 receives pagefaults from me - for NULL pointer detection */
	l4_umword_t d;
	l4_threadid_t preempter = L4_INVALID_ID;
	l4_threadid_t pager = native_thread_id();
	l4_thread_ex_regs(l4_myself(), ~0UL, ~0UL, &preempter, &pager, &d, &d, &d);
}


Core::Platform::Core_pager &Core::Platform::core_pager()
{
	static Core_pager _core_pager(core_pd());
	return _core_pager;
}


/***********************************
 ** Helper for L4 region handling **
 ***********************************/

struct Region
{
	addr_t start;
	addr_t end;

	Region() : start(0), end(0) { }
	Region(addr_t s, addr_t e) : start(s), end(e) { }

	/**
	 * Returns true if the specified range intersects with the region
	 */
	bool intersects(addr_t base, size_t size) const
	{
		return (((base + size) > start) && (base < end));
	}
};


/**
 * Add region to allocator
 */
static inline void add_region(Region r, Range_allocator &alloc)
{
	/* adjust region */
	addr_t start = trunc_page(r.start);
	addr_t end   = round_page(r.end);

	alloc.add_range(start, end - start);
}


/**
 * Remove region from allocator
 */
static inline void remove_region(Region r, Range_allocator &alloc)
{
	/* adjust region */
	addr_t start = trunc_page(r.start);
	addr_t end   = round_page(r.end);

	alloc.remove_range(start, end - start);
}


/**
 * Request any RAM page from Sigma0
 */
static inline int sigma0_req_region(addr_t *addr, unsigned log2size)
{
	/* XXX sigma0 always maps pages RW */
	l4_umword_t req_fpage = l4_fpage(0, log2size, 0, 0).fpage;
	void* rcv_window = L4_IPC_MAPMSG(0, L4_WHOLE_ADDRESS_SPACE);
	addr_t base;
	l4_fpage_t rcv_fpage;
	l4_msgdope_t result;
	l4_msgtag_t tag;

	int err = l4_ipc_call_tag(sigma0_threadid,
	                          L4_IPC_SHORT_MSG, SIGMA0_REQ_FPAGE_ANY, req_fpage,
	                          l4_msgtag(L4_MSGTAG_SIGMA0, 0, 0, 0),
	                          rcv_window, &base, (l4_umword_t *)&rcv_fpage,
	                          L4_IPC_NEVER, &result, &tag);
	int ret = (err || !l4_ipc_fpage_received(result));

	if (!ret) touch_rw((void *)addr, 1);

	*addr = base;
	return ret;
}


void Core::Platform::_setup_mem_alloc()
{
	/*
	 * Completely map program image by touching all pages read-only to
	 * prevent sigma0 from handing out those page as anonymous memory.
	 */
	volatile const char *beg, *end;
	beg = (const char *)(((unsigned)&_prog_img_beg) & L4_PAGEMASK);
	end = (const char *)&_prog_img_end;
	for ( ; beg < end; beg += L4_PAGESIZE) (void)(*beg);

	/* request pages of known page size starting with largest */
	size_t log2_sizes[] = { L4_LOG2_SUPERPAGESIZE, L4_LOG2_PAGESIZE };

	for (unsigned i = 0; i < sizeof(log2_sizes)/sizeof(*log2_sizes); ++i) {
		size_t log2_size = log2_sizes[i];
		size_t size      = 1 << log2_size;

		int err = 0;
		addr_t addr;
		Region region;

		/* request any page of current size from sigma0 */
		do {
			err = sigma0_req_region(&addr, log2_size);
			if (!err) {
				/* XXX do not allocate page0 */
				if (addr == 0) {
					l4_fpage_unmap(l4_fpage(0, log2_size, 0, 0),
					               L4_FP_FLUSH_PAGE | L4_FP_ALL_SPACES);
					continue;
				}

				region.start = addr; region.end = addr + size;
				if (!region.intersects(stack_area_virtual_base(),
				                       stack_area_virtual_size())) {
					add_region(region, _ram_alloc);
					add_region(region, _core_address_ranges());
				}
				remove_region(region, _io_mem_alloc);
				remove_region(region, _region_alloc);
			}
		} while (!err);
	}
}


void Core::Platform::_setup_irq_alloc()
{
	_irq_alloc.add_range(0, 0x10);
}


static l4_kernel_info_t *get_kip()
{
	static l4_kernel_info_t *kip = nullptr;

	if (kip) return kip;

	int err;

	/* region allocator is not setup yet */

	/* map KIP one-to-one */
	void *fpage = L4_IPC_MAPMSG(0, L4_WHOLE_ADDRESS_SPACE);
	l4_umword_t dw0, dw1;
	l4_msgdope_t r;
	l4_msgtag_t tag;

	err = l4_ipc_call_tag(sigma0_threadid,
	                      L4_IPC_SHORT_MSG, SIGMA0_REQ_KIP, 0,
	                      l4_msgtag(L4_MSGTAG_SIGMA0, 0, 0, 0),
	                      fpage, &dw0, &dw1,
	                      L4_IPC_NEVER, &r, &tag);

	bool amok = false;
	if (err) {
		raw("IPC error ", err, " while accessing the KIP");
		amok = true;
	}
	if (!l4_ipc_fpage_received(r)) {
		warning("No fpage received");
		amok = true;
	}

	if (amok)
		panic("kip mapping failed");

	/* store mapping base from received mapping */
	kip = (l4_kernel_info_t *)dw0;

	if (kip->magic != L4_KERNEL_INFO_MAGIC)
		panic("Sigma0 mapped something but not the KIP");

	return kip;
}


void Core::Platform::_setup_basics()
{
	l4_kernel_info_t * kip = get_kip();

	/* parse memory descriptors - look for virtual memory configuration */
	/* XXX we support only one VM region (here and also inside RM) */
	using L4::Kip::Mem_desc;

	_vm_start = 0; _vm_size  = 0;
	Mem_desc *desc = Mem_desc::first(kip);

	for (unsigned i = 0; i < Mem_desc::count(kip); ++i)
		if (desc[i].is_virtual()) {
			_vm_start = round_page(desc[i].start());
			_vm_size  = trunc_page(desc[i].end() - _vm_start + 1);

			break;
		}

	if (_vm_size == 0)
		panic("Virtual memory configuration not found");

	/* configure applicable address space but never use page0 */
	_vm_size  = _vm_start == 0 ? _vm_size - L4_PAGESIZE : _vm_size;
	_vm_start = _vm_start == 0 ? L4_PAGESIZE : _vm_start;
	_region_alloc.add_range(_vm_start, _vm_size);

	/* preserve stack area in core's virtual address space */
	_region_alloc.remove_range(stack_area_virtual_base(),
	                           stack_area_virtual_size());

	/* I/O memory could be the whole user address space */
	/* FIXME if the kernel helps to find out max address - use info here */
	_io_mem_alloc.add_range(0, ~0);

	/* remove KIP area from region and IO_MEM allocator */
	remove_region(Region((addr_t)kip, (addr_t)kip + L4_PAGESIZE), _region_alloc);
	remove_region(Region((addr_t)kip, (addr_t)kip + L4_PAGESIZE), _io_mem_alloc);

	/* remove core program image memory from region and IO_MEM allocator */
	addr_t img_start = (addr_t) &_prog_img_beg;
	addr_t img_end   = (addr_t) &_prog_img_end;
	remove_region(Region(img_start, img_end), _region_alloc);
	remove_region(Region(img_start, img_end), _io_mem_alloc);

	/* image is accessible by core */
	add_region(Region(img_start, img_end), _core_address_ranges());
}


Core::Platform::Platform()
:
	_ram_alloc(nullptr), _io_mem_alloc(&core_mem_alloc()),
	_io_port_alloc(&core_mem_alloc()), _irq_alloc(&core_mem_alloc()),
	_region_alloc(&core_mem_alloc()),
	_kip_rom(_rom_fs, "l4v2_kip", (addr_t)get_kip(), L4_PAGESIZE)
{
	/*
	 * We must be single-threaded at this stage and so this is safe.
	 */
	static bool initialized = 0;
	if (initialized) panic("Platform constructed twice!");
	initialized = true;

	_setup_basics();
	_setup_mem_alloc();
	_setup_io_port_alloc();
	_setup_irq_alloc();
	_init_rom_modules();

	log(_rom_fs);

	l4_threadid_t myself = l4_myself();

	Platform_pd::init();

	/* setup pd object for core pd */
	_core_label[0] = 0;
	_core_pd = new (core_mem_alloc()) Platform_pd(_core_label, myself.id.task);

	/*
	 * We setup the thread object for thread0 in core pd using a special
	 * interface that allows us to specify the lthread number.
	 */
	Platform_thread &core_thread = *new (core_mem_alloc())
		Platform_thread("core.main");

	core_thread.pager(sigma0());
	_core_pd->bind_thread(core_thread);

	/* we never call _core_thread.start(), so set name directly */
	fiasco_register_thread_name(core_thread.native_thread_id(),
	                            core_thread.name().string());

	auto export_page_as_rom_module = [&] (auto rom_name, auto content_fn)
	{
		size_t const size = 1 << get_page_size_log2();
		ram_alloc().alloc_aligned(size, get_page_size_log2()).with_result(

			[&] (void *phys_ptr) {

				/* core-local memory is one-to-one mapped physical RAM */
				addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);
				void * const core_local_ptr = phys_ptr;

				region_alloc().remove_range((addr_t)core_local_ptr, size);
				memset(core_local_ptr, 0, size);
				content_fn(core_local_ptr, size);

				new (core_mem_alloc())
					Rom_module(_rom_fs, rom_name, phys_addr, size);
			},
			[&] (Range_allocator::Alloc_error) {
				warning("failed to export ", rom_name, " as ROM module"); }
		);
	};

	/* core log as ROM module */
	export_page_as_rom_module("core_log",
		[&] (void *core_local_ptr, size_t size) {
			init_core_log(Core_log_range { (addr_t)core_local_ptr, size } ); });

	/* export platform specific infos */
	export_page_as_rom_module("platform_info",
		[&] (void *core_local_ptr, size_t size) {
			Xml_generator xml(reinterpret_cast<char *>(core_local_ptr),
			                  size, "platform_info",
				[&] () {
					xml.node("kernel", [&] () {
						xml.attribute("name", "fiasco"); }); }); });
}


/********************************
 ** Generic platform interface **
 ********************************/

void Core::Platform::wait_for_exit()
{
	/*
	 * On Fiasco, Core never exits. So let us sleep forever.
	 */
	sleep_forever();
}

