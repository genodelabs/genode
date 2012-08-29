/*
 * \brief   Pistachio platform interface implementation
 * \author  Christian Helmuth
 * \date    2006-04-11
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
#include <base/capability.h>
#include <util/misc_math.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>
#include <pistachio/thread_helper.h>
#include <pistachio/kip.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/kip.h>
#include <l4/sigma0.h>
#include <l4/space.h>
#include <l4/bootinfo.h>
#include <l4/schedule.h>
}

using namespace Genode;


static const bool verbose              = false;
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

enum { PAGER_STACK_ELEMENTS = 512 };
static unsigned long _core_pager_stack[PAGER_STACK_ELEMENTS];


static inline bool is_write_fault(Pistachio::L4_Word_t flags) {
	return (flags & 2) == 1; }


static bool wait_for_page_fault(Pistachio::L4_ThreadId_t &from,
                                Pistachio::L4_Word_t     &pf_addr,
                                Pistachio::L4_Word_t     &pf_ip,
                                Pistachio::L4_Word_t     &flags)
{
	using namespace Pistachio;

	L4_Accept(L4_UntypedWordsAcceptor);
	L4_MsgTag_t res = L4_Wait(&from);
	L4_Msg_t msg;

	enum { EXPECT = 2 };
	if (L4_IpcFailed(res) || (L4_UntypedWords(res)) != EXPECT) {
		PERR("got %ld words, expected %d", L4_UntypedWords(res), EXPECT);
		return false;
	}
	L4_Store(res, &msg);

	pf_addr = L4_Get(&msg, 0);
	pf_ip   = L4_Get(&msg, 1);
	flags   = res.X.flags;
	return true;
}


static bool reply_and_wait_for_page_fault(Pistachio::L4_ThreadId_t  to,
                                          Pistachio::L4_MapItem_t   item,
                                          Pistachio::L4_ThreadId_t &from,
                                          Pistachio::L4_Word_t     &pf_addr,
                                          Pistachio::L4_Word_t     &pf_ip,
                                          Pistachio::L4_Word_t     &flags)
{
	using namespace Pistachio;

	L4_Msg_t msg;
	L4_Clear(&msg);
	L4_Append(&msg, item);
	L4_Accept(L4_UntypedWordsAcceptor);
	L4_MsgLoad(&msg);

	L4_MsgTag_t res = L4_ReplyWait(to, &from);

	enum { EXPECT = 2 };
	if (L4_IpcFailed(res) || (L4_UntypedWords(res)) != EXPECT) {
		PERR("got %ld words, expected %d", L4_UntypedWords(res), EXPECT);
		return wait_for_page_fault(from, pf_addr, pf_ip, flags);
	}
	L4_Store(res, &msg);

	pf_addr = L4_Get(&msg, 0);
	pf_ip   = L4_Get(&msg, 1);
	flags   = res.X.flags;
	return true;
}


/****************
 ** Core pager **
 ****************/

static void _core_pager_loop()
{
	if (verbose) PDBG("Core pager running.");

	using namespace Pistachio;

	L4_ThreadId_t t;
	L4_Word_t pf_addr, pf_ip;
	L4_Word_t page_size = Genode::get_page_size();
	L4_Word_t flags;
	L4_MapItem_t item;

	bool send_reply = false;
	while (1) {

		if (send_reply)
			reply_and_wait_for_page_fault(t, item, t, pf_addr, pf_ip, flags);
		else
			wait_for_page_fault(t, pf_addr, pf_ip, flags);

#warning "TODO Ignore fault messages from non-core tasks"

		/*
		 * Check for local echo mapping request. To request a local
		 * mappings, a core thread may send an IPC to the core pager with
		 * the message word 1 (which normally carries the pf_ip) set to 0.
		 * The message word 0 contains a pointer to a map item to be used
		 * for the echo reply.
		 */
		if (pf_ip == 0) {
			item = *(L4_MapItem_t *)pf_addr;
			send_reply = true;
			continue;
		}

		PDBG("Got page fault (pf_addr = %08lx, pf_ip = %08lx, flags = %08lx).",
		     pf_addr, pf_ip, flags);
		print_l4_threadid(L4_GlobalId(t));

		/* check for NULL pointer */
		if (pf_addr < page_size) {
			PERR("possible null pointer %s at address %lx at EIP %lx in",
			     is_write_fault(flags) ? "WRITE" : "READ/EXEC", pf_addr, pf_ip);
			print_l4_threadid(t);
			/* do not unblock faulter */
			break;
		} else if (!_core_address_ranges().valid_addr(pf_addr)) {
			/* page-fault address is not in RAM */

			PERR("%s access outside of RAM at %lx IP %lx",
			     is_write_fault(flags) ? "WRITE" : "READ", pf_addr, pf_ip);
			print_l4_threadid(t);
			/* do not unblock faulter */
			break;
		} else if (verbose_core_pf) {
			PDBG("pfa=%lx ip=%lx in", pf_addr, pf_ip);
			print_l4_threadid(t);
		}

		/* my pf handler is sigma0 - just touch the appropriate page */
		L4_Fpage_t res = L4_Sigma0_GetPage(get_sigma0(),
		                                   L4_Fpage(trunc_page(pf_addr), page_size));
		if (L4_IsNilFpage(res)) {
			panic("Unhandled page fault");
		}

		/* answer pagefault */
		L4_Fpage_t fpage = L4_Fpage(pf_addr, page_size);
		fpage += L4_FullyAccessible;
		item = L4_MapItem(fpage, pf_addr);
		send_reply = true;
	}
}


Platform::Sigma0::Sigma0() : Pager_object(0)
{
	cap(Native_capability(Pistachio::get_sigma0(), 0));
}


Platform::Sigma0 *Platform::sigma0()
{
	static Sigma0 _sigma0;
	return &_sigma0;
}


Platform::Core_pager::Core_pager(Platform_pd *core_pd)
:
	Platform_thread("core.pager"), Pager_object(0)
{
	Platform_thread::pager(sigma0());

	core_pd->bind_thread(this);
	cap(Native_capability(native_thread_id(), 0));

	/* stack begins at the top end of the '_core_pager_stack' array */
	void *sp = (void *)&_core_pager_stack[PAGER_STACK_ELEMENTS - 1];
	start((void *)_core_pager_loop, sp);

	/* pager0 receives pagefaults from me - for NULL pointer detection */
	L4_Set_Pager(native_thread_id());
}


Platform::Core_pager *Platform::core_pager()
{
	static Core_pager _core_pager(core_pd());
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
	if (r.start >= r.end) {
		PERR("(start = 0x%08lx, end = 0x%08lx)\n", r.start, r.end);
		panic("add_region called with bogus parameters.");
	}

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
	if (r.start >= r.end)
		panic("remove_region called with bogus parameters.");

	if (verbose_region_alloc) {
		printf("%p remove: ", &alloc); print_region(r); printf("\n");
	}

	/* adjust region */
	addr_t start = trunc_page(r.start);
	addr_t end   = round_page(r.end);

	alloc.remove_range(start, end - start);
}


static void dump_kip_memdesc(Pistachio::L4_KernelInterfacePage_t *kip)
{
	using namespace Pistachio;

	L4_Word_t num_desc = L4_NumMemoryDescriptors(kip);
	static const char *types[16] =
	{
		"Undefined", "Conventional", "Reserved by kernel",
		"Dedicated", "Shared", "?", "?", "?", "?", "?", 
		"?", "?", "?", "?", "Boot loader",
		"Architecture-dependent"
	};

	for (L4_Word_t i = 0; i < num_desc; i++) {
		L4_MemoryDesc_t *d = L4_MemoryDesc(kip, i);

		printf("mem %ld: [0x%08lx, 0x%08lx) type=0x%lx (%s) %s\n",
		       i,
		       L4_Low(d),
		       L4_High(d)+1,
		       L4_Type(d), types[L4_Type(d) & 0xF],
		       L4_IsVirtual(d) ? "Virtual" : "Non-Virtual");
	}
}


/**
 * Request any RAM page from Sigma0
 */
bool sigma0_req_region(addr_t *addr, unsigned log2size)
{
	using namespace Pistachio;

	L4_Fpage_t fpage = L4_Sigma0_GetAny(get_sigma0(), log2size,
	                                    L4_CompleteAddressSpace);

	if (L4_IsNilFpage(fpage))
		return false;

	*addr = L4_Address(fpage);
	return true;
}


void Platform::_setup_mem_alloc()
{
	/*
	 * Completely map program image by touching all pages read-only to
	 * prevent sigma0 from handing out those page as anonymous memory.
	 */
	volatile const char *beg, *end;
	beg = (const char *)(((unsigned)&_prog_img_beg) & get_page_mask());
	end = (const char *)&_prog_img_end;
	for ( ; beg < end; beg += get_page_size()) (void)(*beg);

	Pistachio::L4_Word_t page_size_mask = Pistachio::L4_PageSizeMask(Pistachio::get_kip());
	unsigned int size_log2;

	/*
	 * Allocate all memory from sigma0 in descending page sizes. Only
	 * try page sizes that are hardware supported.
	 */
	for ( size_log2 = 31; page_size_mask != 0; size_log2-- ) {

		unsigned int size = 1 << size_log2;

		/* if this page size is not supported try next */
		if ((page_size_mask & size) == 0)
			continue;

		/* mask out that size bit */
		page_size_mask &= ~size;

		printf("Trying to allocate %uK pages from sigma0.\n", size >> 10);

		/*
		 * Suck out sigma0. The spec says that we get only "conventional
		 * memory". Let's hope this is true.
		 */
		bool succ;
		unsigned int bytes_got = 0;
		do {
			addr_t addr;
			Region region;

			succ = sigma0_req_region(&addr, size_log2);
			if (succ) {
				/* XXX do not allocate page0 */
				if (addr == 0) {
//					L4_Fpage_t f = L4_FpageLog2(0, pslog2);
//					f += L4_FullyAccessible;
//					L4_Flush(f);

				} else {
					region.start = addr; region.end = addr + size;
					add_region(region, _ram_alloc);
					add_region(region, _core_address_ranges());
					remove_region(region, _io_mem_alloc);
					remove_region(region, _region_alloc);
				}

				bytes_got += size;
			}
		} while (succ);

		printf("Got %uK in %uK pieces.\n", bytes_got >> 10, size >> 10);
	}
}


void Platform::_setup_irq_alloc() { _irq_alloc.add_range(0, 0x10); }


void Platform::_setup_preemption()
{
	/*
	 * The roottask has the maximum priority
	 */
	L4_Set_Priority(Pistachio::L4_Myself(),
	                Platform_thread::DEFAULT_PRIORITY);
}


void Platform::_setup_basics()
{
	using namespace Pistachio;

	/* completely map program image */
	addr_t beg = trunc_page((addr_t)&_prog_img_beg);
	addr_t end = round_page((addr_t)&_prog_img_end);
	for ( ; beg < end; beg += get_page_size())
		L4_Sigma0_GetPage(get_sigma0(), L4_Fpage(beg, get_page_size()));

	/* store mapping base from received mapping */
	L4_KernelInterfacePage_t *kip = (L4_KernelInterfacePage_t *)get_kip();

	if (kip->magic != L4_MAGIC)
		panic("we got something but not the KIP");

	if (verbose) {
		printf("\n");
		printf("KIP @ %p\n", kip);
		printf("    magic: %08lx\n", kip->magic);
		printf("  version: %08lx\n", kip->ApiVersion.raw);
		printf(" BootInfo: %08lx\n", kip->BootInfo);
	}

	dump_kip_memdesc(kip);

	/* add KIP as ROM module */
	_kip_rom = Rom_module((addr_t)kip, sizeof(L4_KernelInterfacePage_t), "pistachio_kip");
	_rom_fs.insert(&_kip_rom);

	/* update multi-boot info pointer from KIP */
	void *mb_info_ptr = (void *)kip->BootInfo;

	// Get virtual bootinfo address.

	L4_Fpage_t bipage = L4_Sigma0_GetPage(get_sigma0(),
	                                      L4_Fpage(kip->BootInfo,
	                                      get_page_size()));
	if (L4_IsNilFpage(bipage))
		panic("Could not map BootInfo.");

	if (!L4_BootInfo_Valid(mb_info_ptr))
		panic("No valid boot info.");

	if (L4_BootInfo_Size(mb_info_ptr) > get_page_size())
		panic("TODO Our multiboot info is bigger than a page...");

	/* done magic */

	_mb_info = Multiboot_info(mb_info_ptr);
	if (verbose) printf("MBI @ %p\n", mb_info_ptr);

	/* get UTCB memory */
	Platform_pd::touch_utcb_space();

	/* I/O memory could be the whole user address space */
	_io_mem_alloc.add_range(0, ~0);

	unsigned int kip_size = sizeof(L4_KernelInterfacePage_t);

	_vm_start = 0x0;
	_vm_size  = 0x0;

	/*
	 * Determine the valid virtual address range by iterating through the
	 * memory descriptors provided by the KIP. We expect only one
	 * virtual-memory descriptor.
	 */
	for (unsigned i = 0; i < L4_NumMemoryDescriptors(kip); i++) {
		L4_MemoryDesc_t *md = L4_MemoryDesc(kip, i);
		if (!L4_IsVirtual(md)) continue;

		if (_vm_start != 0x0 || _vm_size != 0x0) {
			PWRN("KIP has multiple virtual-memory descriptors. Taking only the first.");
			break;
		}

		/*
		 * Exclude the zero page so that we are able to see null-pointer
		 * dereference bugs.
		 */
		_vm_start = max((L4_Word_t)0x1000, L4_MemoryDescLow(md));
		_vm_size  = L4_MemoryDescHigh(md) - _vm_start + 1;

		printf("KIP reports virtual memory region at [%lx,%lx)\n",
		       L4_MemoryDescLow(md), L4_MemoryDescHigh(md));
	}

	/* configure core's virtual memory, exclude KIP, context area */
	_region_alloc.add_range(_vm_start, _vm_size);
	_region_alloc.remove_range((addr_t)kip, kip_size);
	_region_alloc.remove_range(Native_config::context_area_virtual_base(),
	                           Native_config::context_area_virtual_size());

	/* remove KIP and MBI area from region and IO_MEM allocator */
	remove_region(Region((addr_t)kip, (addr_t)kip + kip_size), _region_alloc);
	remove_region(Region((addr_t)kip, (addr_t)kip + kip_size), _io_mem_alloc);
	remove_region(Region((addr_t)mb_info_ptr, (addr_t)mb_info_ptr + _mb_info.size()), _region_alloc);
	remove_region(Region((addr_t)mb_info_ptr, (addr_t)mb_info_ptr + _mb_info.size()), _io_mem_alloc);

	/* remove utcb area */
	addr_t utcb_ptr = (addr_t)Platform_pd::_core_utcb_ptr;

	remove_region(Region(utcb_ptr, utcb_ptr + L4_UtcbAreaSize (kip)), _region_alloc);
	remove_region(Region(utcb_ptr, utcb_ptr + L4_UtcbAreaSize (kip)), _io_mem_alloc);

	/* remove core program image memory from region allocator */
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

	Pistachio::L4_Word_t page_size = Pistachio::get_page_size();

	for (unsigned i = 0; i < _mb_info.num_modules();  i++) {
		if (!(rom = _mb_info.get_module(i)).valid()) continue;

		Rom_module *new_rom = new(core_mem_alloc()) Rom_module(rom);

		_rom_fs.insert(new_rom);

		if (verbose)
			printf(" mod[%d] [%p,%p) %s\n", i,
			       (void *)new_rom->addr(), ((char *)new_rom->addr()) + new_rom->size(),
			       new_rom->name());

		/* zero remainder of last ROM page */
		size_t count = page_size - rom.size() % page_size;
		if (count != page_size)
			memset(reinterpret_cast<void *>(rom.addr() + rom.size()), 0, count);

		/* remove ROM area from region and IO_MEM allocator */
		remove_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _region_alloc);
		remove_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _io_mem_alloc);

		/* add area to core-accessible ranges */
		add_region(Region(new_rom->addr(), new_rom->addr() + new_rom->size()), _core_address_ranges());
	}
}


Platform_pd *Platform::core_pd()
{
	/* on first call, setup task object for core task */
	static Platform_pd _core_pd(true);
	return &_core_pd;
}


Platform::Platform() :
	_ram_alloc(0), _io_mem_alloc(core_mem_alloc()),
	_io_port_alloc(core_mem_alloc()), _irq_alloc(core_mem_alloc()),
	_region_alloc(core_mem_alloc())
{
	/*
	 * We must be single-threaded at this stage and so this is safe.
	 */
	static bool initialized = 0;
	if (initialized) panic("Platform constructed twice!");
	initialized = true;

	_setup_basics();
	_setup_preemption();
	_setup_mem_alloc();
	_setup_io_port_alloc();
	_setup_irq_alloc();
	_setup_rom();

	/*
	 * When dumping 'ram_alloc', there are several small blocks in addition
	 * to the available free memory visible. These small blocks are used to
	 * hold the meta data for the ROM modules as initialized by '_setup_rom'.
	 */
	if (verbose) {
		printf(":ram_alloc: ");    _ram_alloc.raw()->dump_addr_tree();
		printf(":region_alloc: "); _region_alloc.raw()->dump_addr_tree();
		printf(":io_mem: ");       _io_mem_alloc.raw()->dump_addr_tree();
		printf(":io_port: ");      _io_port_alloc.raw()->dump_addr_tree();
		printf(":irq: ");          _irq_alloc.raw()->dump_addr_tree();
		printf(":rom_fs: ");       _rom_fs.print_fs();
		printf(":core ranges: ");  _core_address_ranges().raw()->dump_addr_tree();
	}

	/*
	 * We setup the thread object for thread0 in core task using a
	 * special interface that allows us to specify the thread
	 * ID. For core, this creates the situation that task_id ==
	 * thread_id of first task. But since we do not destroy this
	 * task, it should be no problem.
	 */
	static Platform_thread core_thread("core.main");

	core_thread.set_l4_thread_id(Pistachio::L4_MyGlobalId());
	core_thread.pager(sigma0());

	core_pd()->bind_thread(&core_thread);
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit()
{
	/*
	 * On Pistachio, core never exits. So let us sleep forever.
	 */
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
