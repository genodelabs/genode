/*
 * \brief  Fiasco platform interface implementation
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-04-11
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/allocator_avl.h>
#include <base/crt0.h>
#include <base/sleep.h>
#include <util/misc_math.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>
#include <multiboot.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sigma0/sigma0.h>
#include <l4/sys/ipc.h>
#include <l4/sys/kip>
#include <l4/sys/thread.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>

static l4_kernel_info_t *kip;
}

using namespace Genode;


static const bool verbose              = true;
static const bool verbose_core_pf      = false;
static const bool verbose_region_alloc = false;


/***********************************
 ** Core address space management **
 ***********************************/

static Synchronized_range_allocator<Allocator_avl> &_core_address_ranges()
{
	static Synchronized_range_allocator<Allocator_avl> _core_address_ranges(0);
	return _core_address_ranges;
}

enum { PAGER_STACK_ELEMENTS = 1024 };
static unsigned long _core_pager_stack[PAGER_STACK_ELEMENTS];

/**
 * Core pager "service loop"
 */
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
			PWRN("Received something different than a pagefault, ignoring ...");
			continue;
		}

		/* read fault information */
		l4_umword_t pfa = l4_trunc_page(l4_utcb_mr()->mr[0]);
		l4_umword_t ip  = l4_utcb_mr()->mr[1];
		bool rw         = l4_utcb_mr()->mr[0] & 2; //TODO enum

		if (pfa < (l4_umword_t)L4_PAGESIZE) {

			/* NULL pointer access */
			PERR("Possible null pointer %s at %lx IP %lx",
			     rw ? "WRITE" : "READ", pfa, ip);
			/* do not unblock faulter */
			send_reply = false;
			continue;
		} else if (!_core_address_ranges().valid_addr(pfa)) {

			/* page-fault address is not in RAM */
			PERR("%s access outside of RAM at %lx IP %lx",
			     rw ? "WRITE" : "READ", pfa, ip);
			/* do not unblock faulter */
			send_reply = false;
			continue;
		} else if (verbose_core_pf)
			PDBG("pfa=%lx ip=%lx", pfa, ip);

		/* my pf handler is sigma0 - just touch the appropriate page */
		if (rw)
			touch_rw((void *)pfa, 1);
		else
			touch_ro((void *)pfa, 1);

		send_reply = true;
	}
}


Platform::Sigma0::Sigma0(Cap_index* i) : Pager_object(0)
{
	/*
	 * We use the Pager_object here in a slightly different manner,
	 * just to tunnel the pager cap to the Platform_thread::start method.
	 */
	cap(i);
}


Platform::Core_pager::Core_pager(Platform_pd *core_pd, Sigma0 *sigma0)
: Platform_thread("core.pager"), Pager_object(0)
{
	Platform_thread::pager(sigma0);

	core_pd->bind_thread(this);
	cap(thread().local);

	/* stack begins at the top end of the '_core_pager_stack' array */
	void *sp = (void *)&_core_pager_stack[PAGER_STACK_ELEMENTS - 1];
	start((void *)_core_pager_loop, sp);

	using namespace Fiasco;

	l4_thread_control_start();
	l4_thread_control_pager(thread().local.dst());
	l4_thread_control_exc_handler(thread().local.dst());
	l4_msgtag_t tag = l4_thread_control_commit(L4_BASE_THREAD_CAP);
	if (l4_msgtag_has_error(tag))
		PWRN("l4_thread_control_commit failed!");
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
};


/**
 * Log region
 */
static inline void print_region(Region r)
{
	printf("[%08lx,%08lx) %08lx", r.start, r.end, r.end - r.start);
}


/**
 * Add region to allocator
 */
static inline void add_region(Region r, Range_allocator &alloc)
{
	if (verbose_region_alloc) {
		printf("%p    add: ", &alloc); print_region(r); printf("\n");
	}

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
	if (verbose_region_alloc) {
		printf("%p remove: ", &alloc); print_region(r); printf("\n");
	}

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

	touch_rw((void *)addr, 1);

	return 0;
}


static Fiasco::l4_kernel_info_t *sigma0_map_kip()
{
	using namespace Fiasco;

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
	return (l4_kernel_info_t*) ret;
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
		size_t size      = 1 << log2_size;
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
				add_region(region, _ram_alloc);
				add_region(region, _core_address_ranges());
				remove_region(region, _io_mem_alloc);
				remove_region(region, _region_alloc);
			}
		} while (!err);
	}
}


void Platform::_setup_irq_alloc() { _irq_alloc.add_range(0, 0x100); }


void Platform::_setup_basics()
{
	using namespace Fiasco;

	kip = sigma0_map_kip();
	if (!kip)
		panic("kip mapping failed");

	if (kip->magic != L4_KERNEL_INFO_MAGIC)
		panic("Sigma0 mapped something but not the KIP");

	if (verbose) {
		printf("\n");
		printf("KIP @ %p\n", kip);
		printf("    magic: %08zx\n", (size_t)kip->magic);
		printf("  version: %08zx\n", (size_t)kip->version);
		printf("         sigma0 "); printf(" esp: %08lx  eip: %08lx\n", kip->sigma0_esp, kip->sigma0_eip);
		printf("         sigma1 "); printf(" esp: %08lx  eip: %08lx\n", kip->sigma1_esp, kip->sigma1_eip);
		printf("           root "); printf(" esp: %08lx  eip: %08lx\n", kip->root_esp, kip->root_eip);
	}

	/* add KIP as ROM module */
	_kip_rom = Rom_module((addr_t)kip, L4_PAGESIZE, "l4v2_kip");
	_rom_fs.insert(&_kip_rom);

	/* update multi-boot info pointer from KIP */
	void *mb_info_ptr = (void *)kip->user_ptr;
	_mb_info = Multiboot_info(mb_info_ptr);
	if (verbose) printf("MBI @ %p\n", mb_info_ptr);

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

	/* preserve context area in core's virtual address space */
	_region_alloc.remove_range(Thread_base::CONTEXT_AREA_VIRTUAL_BASE,
	                           Thread_base::CONTEXT_AREA_VIRTUAL_SIZE);

	/* preserve utcb- area in core's virtual address space */
	_region_alloc.remove_range((addr_t)l4_utcb(), L4_PAGESIZE);

	/* I/O memory could be the whole user address space */
	/* FIXME if the kernel helps to find out max address - use info here */
	_io_mem_alloc.add_range(0, ~0);

	/* remove KIP and MBI area from region and IO_MEM allocator */
	remove_region(Region((addr_t)kip, (addr_t)kip + L4_PAGESIZE), _region_alloc);
	remove_region(Region((addr_t)kip, (addr_t)kip + L4_PAGESIZE), _io_mem_alloc);
	remove_region(Region((addr_t)mb_info_ptr, (addr_t)mb_info_ptr + _mb_info.size()), _region_alloc);
	remove_region(Region((addr_t)mb_info_ptr, (addr_t)mb_info_ptr + _mb_info.size()), _io_mem_alloc);

	/* remove core program image memory from region and IO_MEM allocator */
	addr_t img_start = (addr_t) &_prog_img_beg;
	addr_t img_end   = (addr_t) &_prog_img_end;
	remove_region(Region(img_start, img_end), _region_alloc);
	remove_region(Region(img_start, img_end), _io_mem_alloc);

	/* image is accessible by core */
	add_region(Region(img_start, img_end), _core_address_ranges());
}


void Platform::_setup_rom()
{
	Rom_module rom;

	for (unsigned i = FIRST_ROM; i < _mb_info.num_modules();  i++) {
		if (!(rom = _mb_info.get_module(i)).valid()) continue;

		Rom_module *new_rom = new(core_mem_alloc()) Rom_module(rom);
		_rom_fs.insert(new_rom);

		/* map module */
		touch_ro((const void*)new_rom->addr(), new_rom->size());

		if (verbose)
			printf(" mod[%d] [%p,%p) %s\n", i,
			       (void *)new_rom->addr(), ((char *)new_rom->addr()) + new_rom->size(),
			       new_rom->name());

		/* zero remainder of last ROM page */
		size_t count = L4_PAGESIZE - rom.size() % L4_PAGESIZE;
		if (count != L4_PAGESIZE)
			memset(reinterpret_cast<void *>(rom.addr() + rom.size()), 0, count);

		/* remove ROM area from region and IO_MEM allocator */
		remove_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _region_alloc);
		remove_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _io_mem_alloc);

		/* add area to core-accessible ranges */
		add_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _core_address_ranges());
	}

	Rom_module *kip_rom = new(core_mem_alloc())
		Rom_module((addr_t)Fiasco::kip, L4_PAGESIZE, "kip");
	_rom_fs.insert(kip_rom);
}


Platform::Platform() :
	_ram_alloc(0), _io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()), _irq_alloc(core_mem_alloc()),
	_region_alloc(core_mem_alloc()), _cap_id_alloc(core_mem_alloc()),
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
	_setup_rom();

	if (verbose) {
		printf(":ram_alloc: ");    _ram_alloc.raw()->dump_addr_tree();
		printf(":region_alloc: "); _region_alloc.raw()->dump_addr_tree();
		printf(":io_mem: ");       _io_mem_alloc.raw()->dump_addr_tree();
		printf(":io_port: ");      _io_port_alloc.raw()->dump_addr_tree();
		printf(":irq: ");          _irq_alloc.raw()->dump_addr_tree();
		printf(":rom_fs: ");       _rom_fs.print_fs();
		printf(":core ranges: ");  _core_address_ranges().raw()->dump_addr_tree();
	}

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


void Core_parent::exit(int exit_value) { }
