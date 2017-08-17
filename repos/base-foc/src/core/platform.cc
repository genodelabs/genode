/*
 * \brief  Fiasco platform interface implementation
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/allocator_avl.h>
#include <base/sleep.h>
#include <util/misc_math.h>

/* base-internal includes */
#include <base/internal/crt0.h>
#include <base/internal/stack_area.h>
#include <base/internal/globals.h>

/* core includes */
#include <boot_modules.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sigma0/sigma0.h>
#include <l4/sys/icu.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kip>
#include <l4/sys/thread.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/scheduler.h>
}

using namespace Genode;


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

/**
 * Core pager "service loop"
 */

/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
static void _core_pager_loop()
{
	using namespace Fiasco;

	bool send_reply = false;
	l4_umword_t  label;
	l4_utcb_t   *utcb    = l4_utcb();
	l4_msgtag_t  snd_tag = l4_msgtag(0, 0, 0, 0);
	l4_msgtag_t  tag;

	while (true) {

		if (send_reply)
			tag = l4_ipc_reply_and_wait(utcb, snd_tag, &label, L4_IPC_NEVER);
		else
			tag = l4_ipc_wait(utcb, &label, L4_IPC_NEVER);

		if (!tag.is_page_fault()) {
			warning("received something different than a pagefault, ignoring ...");
			continue;
		}

		/* read fault information */
		l4_umword_t pfa = l4_trunc_page(l4_utcb_mr()->mr[0]);
		l4_umword_t ip  = l4_utcb_mr()->mr[1];
		bool rw         = l4_utcb_mr()->mr[0] & 2; //TODO enum

		if (pfa < (l4_umword_t)L4_PAGESIZE) {

			/* NULL pointer access */
			error("Possible null pointer ", rw ? "WRITE" : "READ", " "
			      "at ", Hex(pfa), " IP ", Hex(ip));
			/* do not unblock faulter */
			send_reply = false;
			continue;
		} else if (!_core_address_ranges().valid_addr(pfa)) {

			/* page-fault address is not in RAM */
			error(rw ? "WRITE" : "READ", " access outside of RAM "
			      "at ", Hex(pfa), " IP ", Hex(ip));
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


Platform::Sigma0::Sigma0(Cap_index* i)
:
	Pager_object(Cpu_session_capability(), Thread_capability(),
	             0, Affinity::Location(), Session_label(),
	             Cpu_session::Name("sigma0"))
{
	/*
	 * We use the Pager_object here in a slightly different manner,
	 * just to tunnel the pager cap to the Platform_thread::start method.
	 */
	cap(*i);
}


Platform::Core_pager::Core_pager(Platform_pd *core_pd, Sigma0 *sigma0)
:
	Platform_thread("core.pager"),
	Pager_object(Cpu_session_capability(), Thread_capability(),
	             0, Affinity::Location(), Session_label(),
	             Cpu_session::Name(name()))
{
	Platform_thread::pager(sigma0);

	core_pd->bind_thread(this);
	cap(thread().local);

	/* stack begins at the top end of the '_core_pager_stack' array */
	void *sp = (void *)&_core_pager_stack[PAGER_STACK_ELEMENTS - 1];
	start((void *)_core_pager_loop, sp);

	using namespace Fiasco;

	l4_thread_control_start();
	l4_thread_control_pager(thread().local.data()->kcap());
	l4_thread_control_exc_handler(thread().local.data()->kcap());
	l4_msgtag_t tag = l4_thread_control_commit(L4_BASE_THREAD_CAP);
	if (l4_msgtag_has_error(tag))
		warning("l4_thread_control_commit failed!");
}


Platform::Core_pager *Platform::core_pager()
{
	static Core_pager _core_pager(core_pd(), &_sigma0);
	return &_core_pager;
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
	using namespace Fiasco;

	l4_utcb_mr()->mr[0] = SIGMA0_REQ_FPAGE_ANY;
	l4_utcb_mr()->mr[1] = l4_fpage(0, log2size, 0).raw;

	/* open receive window for mapping */
	l4_utcb_br()->bdr  &= ~L4_BDR_OFFSET_MASK;
	l4_utcb_br()->br[0] = L4_ITEM_MAP;
	l4_utcb_br()->br[1] = l4_fpage(0, L4_WHOLE_ADDRESS_SPACE, L4_FPAGE_RWX).raw;

	l4_msgtag_t tag = l4_msgtag(L4_PROTO_SIGMA0, 2, 0, 0);
	tag = l4_ipc_call(L4_BASE_PAGER_CAP, l4_utcb(), tag, L4_IPC_NEVER);
	if (l4_ipc_error(tag, l4_utcb()))
		return -1;

	if (l4_msgtag_items(tag) != 1)
		return -2;

	*addr = l4_utcb_mr()->mr[0] & (~0UL << L4_PAGESHIFT);

	return 0;
}


static Fiasco::l4_kernel_info_t *sigma0_map_kip()
{
	using namespace Fiasco;

	static l4_kernel_info_t *kip = nullptr;

	if (kip) return kip;

	/* signal we want to map the KIP */
	l4_utcb_mr()->mr[0] = SIGMA0_REQ_KIP;

	/* open receive window for KIP one-to-one */
	l4_utcb_br()->bdr  &= ~L4_BDR_OFFSET_MASK;
	l4_utcb_br()->br[0] = L4_ITEM_MAP;
	l4_utcb_br()->br[1] = l4_fpage(0, L4_WHOLE_ADDRESS_SPACE, L4_FPAGE_RX).raw;

	/* call sigma0 */
	l4_msgtag_t tag = l4_ipc_call(L4_BASE_PAGER_CAP,
	                              l4_utcb(),
	                              l4_msgtag(L4_PROTO_SIGMA0, 1, 0, 0),
	                              L4_IPC_NEVER);
	if (l4_ipc_error(tag, l4_utcb()))
		return 0;

	l4_addr_t ret = l4_trunc_page(l4_utcb_mr()->mr[0]);

	if (!ret)
		panic("kip mapping failed");
	else
		kip = (l4_kernel_info_t*) ret;
	return kip;
}


void Platform::_setup_mem_alloc()
{
	/*
	 * Completely map program image by touching all pages read-only to
	 * prevent sigma0 from handing out those page as anonymous memory.
	 */
	volatile const char *beg, *end;
	beg = (const char *)(((Genode::addr_t)&_prog_img_beg) & L4_PAGEMASK);
	end = (const char *)&_prog_img_end;
	for ( ; beg < end; beg += L4_PAGESIZE) (void)(*beg);

	/* request pages of known page size starting with largest */
	size_t log2_sizes[] = { L4_LOG2_SUPERPAGESIZE, L4_LOG2_PAGESIZE };

	for (unsigned i = 0; i < sizeof(log2_sizes)/sizeof(*log2_sizes); ++i) {
		size_t log2_size = log2_sizes[i];
		size_t size      = 1UL << log2_size;
		int    err       = 0;
		addr_t addr      = 0;
		Region region;

		/* request any page of current size from sigma0 */
		do {
			err = sigma0_req_region(&addr, log2_size);
			if (!err) {
				/* XXX do not allocate page0 */
				if (addr == 0) {
					Fiasco::l4_task_unmap(Fiasco::L4_BASE_TASK_CAP,
					                      Fiasco::l4_fpage(0, log2_size,
					                                       Fiasco::L4_FPAGE_RW),
					                      Fiasco::L4_FP_ALL_SPACES);
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


void Platform::_setup_irq_alloc()
{
	using namespace Fiasco;

	l4_icu_info_t info { .features = 0 };
	l4_msgtag_t res = l4_icu_info(Fiasco::L4_BASE_ICU_CAP, &info);
	if (l4_error(res))
		panic("could not determine number of IRQs");

	_irq_alloc.add_range(0, info.nr_irqs);
}


void Platform::_setup_basics()
{
	using namespace Fiasco;

	l4_kernel_info_t * kip = sigma0_map_kip();

	if (kip->magic != L4_KERNEL_INFO_MAGIC)
		panic("Sigma0 mapped something but not the KIP");

	log("");
	log("KIP @ ", kip);
	log("    magic: ", Hex(kip->magic));
	log("  version: ", Hex(kip->version));

	/* add KIP as ROM module */
	_rom_fs.insert(&_kip_rom);

	/* update multi-boot info pointer from KIP */
	addr_t mb_info_addr = kip->user_ptr;
	log("MBI @ ", Hex(mb_info_addr));

	/* parse memory descriptors - look for virtual memory configuration */
	/* XXX we support only one VM region (here and also inside RM) */
	using Fiasco::L4::Kip::Mem_desc;

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

	/* preserve utcb- area in core's virtual address space */
	_region_alloc.remove_range((addr_t)l4_utcb(), L4_PAGESIZE * 16);

	/* I/O memory could be the whole user address space */
	/* FIXME if the kernel helps to find out max address - use info here */
	_io_mem_alloc.add_range(0, ~0);

	/* remove KIP and MBI area from region and IO_MEM allocator */
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


Platform::Platform() :
	_ram_alloc(nullptr), _io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()), _irq_alloc(core_mem_alloc()),
	_region_alloc(core_mem_alloc()), _cap_id_alloc(core_mem_alloc()),
	_kip_rom(Rom_module((addr_t)sigma0_map_kip(), L4_PAGESIZE, "l4v2_kip")),
	_sigma0(cap_map()->insert(_cap_id_alloc.alloc(), Fiasco::L4_BASE_PAGER_CAP))
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

	log(":ram_alloc: ",    _ram_alloc);
	log(":region_alloc: ", _region_alloc);
	log(":io_mem: ",       _io_mem_alloc);
	log(":io_port: ",      _io_port_alloc);
	log(":irq: ",          _irq_alloc);
	log(":rom_fs: ",       _rom_fs);
	log(":core ranges: ",  _core_address_ranges());

	Core_cap_index* pdi =
		reinterpret_cast<Core_cap_index*>(cap_map()->insert(_cap_id_alloc.alloc(), Fiasco::L4_BASE_TASK_CAP));
	Core_cap_index* thi =
		reinterpret_cast<Core_cap_index*>(cap_map()->insert(_cap_id_alloc.alloc(), Fiasco::L4_BASE_THREAD_CAP));
	Core_cap_index* irqi =
		reinterpret_cast<Core_cap_index*>(cap_map()->insert(_cap_id_alloc.alloc()));

	/* setup pd object for core pd */
	_core_pd = new(core_mem_alloc()) Platform_pd(pdi);

	/*
	 * We setup the thread object for thread0 in core pd using a special
	 * interface that allows us to specify the capability slot.
	 */
	Platform_thread *core_thread = new(core_mem_alloc())
		Platform_thread(thi, irqi, "core.main");

	core_thread->pager(&_sigma0);
	_core_pd->bind_thread(core_thread);
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit()
{
	/*
	 * On Fiasco, Core never exits. So let us sleep forever.
	 */
	sleep_forever();
}


Affinity::Space Platform::affinity_space() const
{
	using namespace Genode;
	using namespace Fiasco;

	l4_sched_cpu_set_t cpus = l4_sched_cpu_set(0, 0, 1);
	l4_umword_t cpus_max;
	l4_msgtag_t res = l4_scheduler_info(L4_BASE_SCHEDULER_CAP, &cpus_max,
	                                    &cpus);
	if (l4_error(res)) {
		error("could not detect number of CPUs - assuming 1 CPU");
		return 1;
	}

	unsigned cpus_online = 0;
	for (unsigned i = 0; i < sizeof(cpus.map) * 8; i++)
		if ((cpus.map >> i) & 0x1)
			cpus_online ++;

	/*
	 * Currently, we do not gather any information about the topology of CPU
	 * nodes but just return a one-dimensional affinity space.
	 */
	return Affinity::Space(cpus_online, 1);
}

