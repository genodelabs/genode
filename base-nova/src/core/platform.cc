/*
 * \brief  Platform interface implementation
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>
#include <nova_util.h>
#include <util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;
using namespace Nova;


enum { verbose_boot_info = true };

Native_utcb *main_thread_utcb();


/**
 * Initial value of esp register, saved by the crt0 startup code.
 * This value contains the address of the hypervisor information page.
 */
extern addr_t __initial_sp;


/**
 * Pointer to the UTCB of the main thread
 */
Utcb *__main_thread_utcb;


/**
 * Virtual address range consumed by core's program image
 */
extern unsigned _prog_img_beg, _prog_img_end;


/**
 *  Capability selector of root PD
 */
addr_t __core_pd_sel;


/**
 * Returns true if the phys address intersects with some other reserved area.
 */
static bool intersects(addr_t const phys, const Hip::Mem_desc * mem_desc_const,
                       size_t const num_mem_desc)
{ 
	/* check whether phys intersects with some reserved area */
	Hip::Mem_desc const * mem_desc = mem_desc_const;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (mem_desc->addr > ~0UL) continue;

		addr_t base = trunc_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = round_page(~0UL - mem_desc->addr + 1);
		else
			size = round_page(mem_desc->addr + mem_desc->size) - base;

		if (base <= phys && phys < base + size)
			return true;
	}

	/* check whether phys is part of available memory */
	mem_desc = mem_desc_const;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (mem_desc->addr > ~0UL) continue;

		addr_t base = trunc_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = round_page(~0UL - mem_desc->addr + 1);
		else
			size = round_page(mem_desc->addr + mem_desc->size) - base;
	
		if (base <= phys && phys < base + size)
			return false;
	}

	return true;
}


/**
 * Map preserved physical page for the exclusive read-execute-only used by core
 */
addr_t Platform::_map_page(addr_t const phys_page, addr_t const pages,
                           bool const extra_page)
{
	addr_t const phys_addr = phys_page << get_page_size_log2();
	addr_t const size = pages << get_page_size_log2();
	addr_t const size_extra = size + (extra_page ? get_page_size() : 0);

	/* try to reserve contiguous virtual area */
	void * core_local_ptr = 0;
	if (!region_alloc()->alloc(size_extra, &core_local_ptr))
		return 0;
	/* free the virtual area, but re allocate it later in two pieces */
	region_alloc()->free(core_local_ptr, size_extra);

	addr_t const core_local_addr = reinterpret_cast<addr_t>(core_local_ptr);

	/* allocate two separate regions - first part */
	if (region_alloc()->alloc_addr(size, core_local_addr).is_error())
		return 0;

	/* allocate two separate regions - second part */
	if (extra_page) {
		/* second region can be freed separately now, if unneeded */
		if (region_alloc()->alloc_addr(get_page_size(), core_local_addr +
		                               size).is_error())
			return 0;
	}

	/* map first part */
	int res = map_local(__main_thread_utcb, phys_addr, core_local_addr, pages,
	                    Nova::Rights(true, true, true), true);

	/* map second part - if requested */
	if (!res && extra_page)
		res = map_local(__main_thread_utcb, phys_addr + size,
	                    core_local_addr + size, 1,
	                    Nova::Rights(true, true, false), true);

	return res ? 0 : core_local_addr;
}


void Platform::_unmap_page(addr_t const phys, addr_t const virt,
                           addr_t const pages)
{
	/* unmap page */
	unmap_local(__main_thread_utcb, virt, pages);
	/* put virtual address back to allocator */
	region_alloc()->free((void *)(virt), pages << get_page_size_log2());
	/* put physical address back to allocator */
	if (phys != 0)
		ram_alloc()->add_range(phys, pages << get_page_size_log2());
}


/*****************************
 ** Core page-fault handler **
 *****************************/


enum { CORE_PAGER_UTCB_ADDR = 0xbff02000 };


/**
 * IDC handler for the page-fault portal
 */
static void page_fault_handler()
{
	Utcb *utcb = (Utcb *)CORE_PAGER_UTCB_ADDR;

	addr_t pf_addr = utcb->qual[1];
	addr_t pf_ip   = utcb->ip;
	addr_t pf_sp   = utcb->sp;
	addr_t pf_type = utcb->qual[0];

	print_page_fault("\nPAGE-FAULT IN CORE", pf_addr, pf_ip,
	                 (Genode::Rm_session::Fault_type)pf_type, ~0UL);

	/* dump stack trace */
	struct Core_img
	{
		addr_t  _beg;
		addr_t  _end;
		addr_t *_ip;

		Core_img(addr_t sp)
		{
			extern addr_t _dtors_end;
			_beg = (addr_t)&_prog_img_beg;
			_end = (addr_t)&_dtors_end;

			_ip = (addr_t *)sp;
			for (;!ip_valid(); _ip++) {}
		}

		addr_t *ip()       { return _ip; }
		void    next_ip()  { _ip = ((addr_t *)*(_ip - 1)) + 1;}
		bool    ip_valid() { return (*_ip >= _beg) && (*_ip < _end); }
	};

	int count = 1;
	printf("  #%d %08lx %08lx\n", count++, pf_sp, pf_ip);

	Core_img dump(pf_sp);
	while (dump.ip_valid()) {
		printf("  #%d %p %08lx\n", count++, dump.ip(), *dump.ip());
		dump.next_ip();
	}

	sleep_forever();
}


static addr_t core_pager_stack_top()
{
	enum { STACK_SIZE = 4*1024 };
	static char stack[STACK_SIZE];
	return (addr_t)&stack[STACK_SIZE - sizeof(addr_t)];
}


/**
 * Startup handler for core threads
 */
static void startup_handler()
{
	Utcb *utcb = (Utcb *)CORE_PAGER_UTCB_ADDR;

	/* initial IP is on stack */
	utcb->ip = *reinterpret_cast<addr_t *>(utcb->sp);
	utcb->mtd = Mtd::EIP | Mtd::ESP;
	utcb->set_msg_word(0);

	reply((void*)core_pager_stack_top());
}


static void init_core_page_fault_handler()
{
	/* create echo EC */
	enum {
		GLOBAL     = false,
		EXC_BASE   = 0
	};

	addr_t ec_sel = cap_map()->insert();

	uint8_t ret = create_ec(ec_sel, __core_pd_sel, boot_cpu(),
	                        CORE_PAGER_UTCB_ADDR, core_pager_stack_top(),
	                        EXC_BASE, GLOBAL);
	if (ret)
		PDBG("create_ec returned %u", ret);

	/* set up page-fault portal */
	create_pt(PT_SEL_PAGE_FAULT, __core_pd_sel, ec_sel,
	          Mtd(Mtd::QUAL | Mtd::ESP | Mtd::EIP),
	          (addr_t)page_fault_handler);
	revoke(Obj_crd(PT_SEL_PAGE_FAULT, 0, Obj_crd::RIGHT_PT_CTRL));

	/* startup portal for global core threads */
	create_pt(PT_SEL_STARTUP, __core_pd_sel, ec_sel,
	          Mtd(Mtd::EIP | Mtd::ESP),
	          (addr_t)startup_handler);
	revoke(Obj_crd(PT_SEL_STARTUP, 0, Obj_crd::RIGHT_PT_CTRL));
}


/**************
 ** Platform **
 **************/

Platform::Platform() :
	_io_mem_alloc(core_mem_alloc()), _io_port_alloc(core_mem_alloc()),
	_irq_alloc(core_mem_alloc()),
	_vm_base(0x1000), _vm_size(0), _cpus(Affinity::Space(1,1))
{
	Hip  *hip  = (Hip *)__initial_sp;
	/* check for right API version */
	if (hip->api_version != 6)
		nova_die();

	/*
	 * Determine number of available CPUs
	 *
	 * XXX As of now, we assume a one-dimensional affinity space, ignoring
	 *     the y component of the affinity location. When adding support
	 *     for two-dimensional affinity spaces, look out and adjust the use of
	 *     'Platform_thread::_location' in 'platform_thread.cc'. Also look
	 *     at the 'Thread_base::start' function in core/thread_start.cc.
	 */
	_cpus = Affinity::Space(hip->cpus(), 1);

	/* register UTCB of main thread */
	__main_thread_utcb = (Utcb *)(__initial_sp - get_page_size());

	/* set core pd selector */
	__core_pd_sel = hip->sel_exc;

	/* create lock used by capability allocator */
	Nova::create_sm(Nova::SM_SEL_EC, __core_pd_sel, 0);

	/* locally map the whole I/O port range */
	enum { ORDER_64K = 16 };
	map_local_one_to_one(__main_thread_utcb, Io_crd(0, ORDER_64K));
	/* map BDA region, console reads IO ports at BDA_VIRT_ADDR + 0x400 */
	enum { BDA_PHY = 0x0U, BDA_VIRT = 0x1U, BDA_VIRT_ADDR = 0x1000U };
	map_local_phys_to_virt(__main_thread_utcb,
	                       Mem_crd(BDA_PHY,  0, Rights(true, false, false)),
	                       Mem_crd(BDA_VIRT, 0, Rights(true, false, false)));


	/*
	 * Now that we can access the I/O ports for comport 0, printf works...
	 */

	/*
	 * remap main utcb to default utcb address
	 * we do this that early, because Core_mem_allocator uses
	 * the main_thread_utcb very early to establish mappings
	 */
	if (map_local(__main_thread_utcb, (addr_t)__main_thread_utcb,
	              (addr_t)main_thread_utcb(), 1, Rights(true, true, false))) {
		PERR("could not remap utcb of main thread");
		nova_die();
	}

	/* sanity checks */
	if (hip->sel_exc + 3 > NUM_INITIAL_PT_RESERVED) {
		printf("configuration error\n");
		nova_die();
	}

	/* configure virtual address spaces */
#ifdef __x86_64__
	_vm_size = 0x7FFFFFFFF000UL - _vm_base;
#else
	_vm_size = 0xc0000000UL - _vm_base;
#endif

	/* set up page fault handler for core - for debugging */
	init_core_page_fault_handler();

	if (verbose_boot_info) {
		printf("Hypervisor %s VMX\n", hip->has_feature_vmx() ? "features" : "does not feature");
		printf("Hypervisor %s SVM\n", hip->has_feature_svm() ? "features" : "does not feature");
		printf("Hypervisor reports %ux%u CPU%c - boot CPU is %lu\n",
		       _cpus.width(), _cpus.height(), _cpus.total() > 1 ? 's' : ' ', boot_cpu());
	}

	/* initialize core allocators */
	size_t const num_mem_desc = (hip->hip_length - hip->mem_desc_offset)
	                            / hip->mem_desc_size;

	if (verbose_boot_info)
		printf("Hypervisor info page contains %zd memory descriptors:\n", num_mem_desc);

	addr_t mem_desc_base = ((addr_t)hip + hip->mem_desc_offset);

	/* define core's virtual address space */
	addr_t virt_beg = _vm_base;
	addr_t virt_end = _vm_size;
	_core_mem_alloc.virt_alloc()->add_range(virt_beg, virt_end - virt_beg);

	/* exclude core image from core's virtual address allocator */
	addr_t core_virt_beg = trunc_page((addr_t)&_prog_img_beg);
	addr_t core_virt_end = round_page((addr_t)&_prog_img_end);
	size_t core_size     = core_virt_end - core_virt_beg;
	region_alloc()->remove_range(core_virt_beg, core_size);

	/* preserve Bios Data Area (BDA) in core's virtual address space */
	region_alloc()->remove_range(BDA_VIRT_ADDR, 0x1000);

	/* preserve context area in core's virtual address space */
	region_alloc()->remove_range(Native_config::context_area_virtual_base(),
	                             Native_config::context_area_virtual_size());

	/* exclude utcb of core pager thread + empty guard pages before and after */
	region_alloc()->remove_range(CORE_PAGER_UTCB_ADDR - get_page_size(),
	                             get_page_size() * 3);

	/* exclude utcb of echo thread + empty guard pages before and after */
	region_alloc()->remove_range(Echo::ECHO_UTCB_ADDR - get_page_size(),
	                             get_page_size() * 3);

	/* exclude utcb of main thread and hip + empty guard pages before and after */
	region_alloc()->remove_range((addr_t)__main_thread_utcb - get_page_size(),
	                             get_page_size() * 4);

	/* sanity checks */
	addr_t check [] = {
		reinterpret_cast<addr_t>(__main_thread_utcb), CORE_PAGER_UTCB_ADDR,
		Echo::ECHO_UTCB_ADDR, BDA_VIRT_ADDR
	};

	for (unsigned i = 0; i < sizeof(check) / sizeof(check[0]); i++) { 
		if (Native_config::context_area_virtual_base() <= check[i] &&
			check[i] < Native_config::context_area_virtual_base() +
			Native_config::context_area_virtual_size())
		{
			PERR("overlapping area - [%lx, %lx) vs %lx",
			     Native_config::context_area_virtual_base(),
			     Native_config::context_area_virtual_base() +
			     Native_config::context_area_virtual_size(), check[i]);
			nova_die();
		}
	}
 
	/* initialize core's physical-memory and I/O memory allocator */
	_io_mem_alloc.add_range(0, ~0xfffUL);
	Hip::Mem_desc *mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		if (verbose_boot_info)
			printf("detected physical memory: 0x%16llx - size: 0x%llx\n",
			        mem_desc->addr, mem_desc->size);

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		addr_t base = round_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = trunc_page(~0UL - mem_desc->addr + 1);
		else
			size = trunc_page(mem_desc->addr + mem_desc->size) - base;

		if (verbose_boot_info)
			printf("use      physical memory: 0x%16lx - size: 0x%zx\n", base, size);

		_io_mem_alloc.remove_range(base, size);
		ram_alloc()->add_range(base, size);
	}

	/* exclude all non-available memory from physical allocator */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type == Hip::Mem_desc::AVAILABLE_MEMORY) continue;

		/* skip regions above 4G on 32 bit, no op on 64 bit */
		if (mem_desc->addr > ~0UL) continue;

		addr_t base = trunc_page(mem_desc->addr);
		size_t size;
		/* truncate size if base+size larger then natural 32/64 bit boundary */
		if (mem_desc->addr >= ~0UL - mem_desc->size + 1)
			size = round_page(~0UL - mem_desc->addr + 1);
		else
			size = round_page(mem_desc->addr + mem_desc->size) - base;

		_io_mem_alloc.add_range(base, size);
		ram_alloc()->remove_range(base, size);
	}

	/* needed as I/O memory by the VESA driver */
	_io_mem_alloc.add_range(0, 0x1000);
	ram_alloc()->remove_range(0, 0x1000);

	/* exclude pages holding multi-boot command lines from core allocators */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	addr_t prev_cmd_line_page = ~0, curr_cmd_line_page = 0;
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->aux) continue;

		curr_cmd_line_page = mem_desc->aux >> get_page_size_log2();
		if (curr_cmd_line_page == prev_cmd_line_page) continue;

		ram_alloc()->remove_range(curr_cmd_line_page << get_page_size_log2(),
		                          get_page_size() * 2);
		prev_cmd_line_page = curr_cmd_line_page;
	}

	/*
	 * From now on, it is save to use the core allocators...
	 */

	/*
	 * Allocate ever an extra page behind the command line pointer. If it turns
	 * out that this page is unused, because the command line was short enough,
	 * the mapping is revoked and the virtual and physical regions are put back
	 * to the allocator.
	 */
	mem_desc = (Hip::Mem_desc *)mem_desc_base;
	prev_cmd_line_page = ~0UL, curr_cmd_line_page = 0;
	addr_t mapped_cmd_line = 0;
	addr_t aux     = ~0UL;
	size_t aux_len = 0;
	/* build ROM file system */
	for (unsigned i = 0; i < num_mem_desc; i++, mem_desc++) {
		if (mem_desc->type != Hip::Mem_desc::MULTIBOOT_MODULE) continue;
		if (!mem_desc->addr || !mem_desc->size || !mem_desc->aux) continue;

		/* convenience */
		addr_t const rom_mem_start = trunc_page(mem_desc->addr);
		addr_t const rom_mem_end   = round_page(mem_desc->addr + mem_desc->size);
		addr_t const rom_mem_size  = rom_mem_end - rom_mem_start;
		bool const aux_in_rom_area = (rom_mem_start <= mem_desc->aux) &&
		                             (mem_desc->aux < rom_mem_end);

		/* map ROM + extra page for the case aux crosses page boundary */
		addr_t core_local_addr = _map_page(rom_mem_start >> get_page_size_log2(),
		                                   rom_mem_size  >> get_page_size_log2(),
		                                   aux_in_rom_area);
		if (!core_local_addr) {
			PERR("could not map multi boot module");
			nova_die();
		}

		/* adjust core_local_addr of module if it was not page aligned */
		core_local_addr += mem_desc->addr - rom_mem_start;

		if (verbose_boot_info)
			printf("map multi-boot module: physical 0x%8lx -> [0x%8lx-0x%8lx)"
			       " - ", (addr_t)mem_desc->addr, (addr_t)core_local_addr,
			       (addr_t)(core_local_addr + mem_desc->size));

		char * name;
		if (aux_in_rom_area) {
			aux = core_local_addr + (mem_desc->aux - mem_desc->addr);
			aux_len = strlen(reinterpret_cast<char const *>(aux)) + 1;

			/* if last page is unused, free it up */
			if (aux + aux_len <= round_page(core_local_addr) + rom_mem_size) {
				bool overlap = intersects(rom_mem_end,
				                          (Hip::Mem_desc *)mem_desc_base,
				                          num_mem_desc);
				_unmap_page(overlap ? 0 : rom_mem_end,
				            round_page(core_local_addr) + rom_mem_size, 1);
			}

			/* all behind rom module will be cleared, copy the command line */
			char *name_tmp = commandline_to_basename(reinterpret_cast<char *>(aux));
			unsigned name_tmp_size = aux_len - (name_tmp - reinterpret_cast<char *>(aux));
			name = new (core_mem_alloc()) char [name_tmp_size];
			memcpy(name, name_tmp, name_tmp_size);

		} else {

			curr_cmd_line_page = mem_desc->aux >> get_page_size_log2();
			if (curr_cmd_line_page != prev_cmd_line_page) {
				int err = 1;
				if (curr_cmd_line_page == prev_cmd_line_page + 1) {
					/* try to allocate subsequent virtual region */
					addr_t const virt = mapped_cmd_line + get_page_size() * 2;
					addr_t const phys = round_page(mem_desc->aux);

					if (region_alloc()->alloc_addr(get_page_size(), virt).is_ok()) {
						/* we got the virtual region */
						err = map_local(__main_thread_utcb, phys, virt, 1,
						                Nova::Rights(true, false, false), true);
						if (!err) {
							/* we got the mapping */
							mapped_cmd_line    += get_page_size();
							prev_cmd_line_page += 1;
						}
					}
				}

				/* allocate new pages if it was not successful beforehand */
				if (err) {
					/* check whether we can free up unused page */
					if (aux + aux_len <= mapped_cmd_line + get_page_size()) {
						addr_t phys = (curr_cmd_line_page + 1) << get_page_size_log2();
						phys = intersects(phys, (Hip::Mem_desc *)mem_desc_base,
						                  num_mem_desc) ? 0 : phys;
						_unmap_page(phys, mapped_cmd_line + get_page_size(), 1);
					}

					mapped_cmd_line = _map_page(curr_cmd_line_page, 1, true);
					prev_cmd_line_page = curr_cmd_line_page;

					if (!mapped_cmd_line) {
						PERR("could not map command line");
						nova_die();
					}
				}
			}
			aux = mapped_cmd_line + (mem_desc->aux - trunc_page(mem_desc->aux));
			aux_len = strlen(reinterpret_cast<char const *>(aux)) + 1;
			name = commandline_to_basename(reinterpret_cast<char *>(aux));

		}

		/* set zero out range */
		addr_t const zero_out = core_local_addr + mem_desc->size;
		/* zero out behind rom module */
		memset(reinterpret_cast<void *>(zero_out), 0, round_page(zero_out) -
		       zero_out);

		printf("%s\n", name);

		/* revoke write permission on rom module */
		unmap_local(__main_thread_utcb, trunc_page(core_local_addr),
		            rom_mem_size >> get_page_size_log2(), true,
		            Nova::Rights(false, true, false));

		/* create rom module */
		Rom_module *rom_module = new (core_mem_alloc())
		                         Rom_module(core_local_addr, mem_desc->size, name);
		_rom_fs.insert(rom_module);
	}

	/* export hypervisor info page as ROM module */
	_rom_fs.insert(new (core_mem_alloc())
	               Rom_module((addr_t)hip, get_page_size(), "hypervisor_info_page"));

	/* I/O port allocator (only meaningful for x86) */
	_io_port_alloc.add_range(0, 0x10000);

	/* IRQ allocator */
	_irq_alloc.add_range(0, hip->sel_gsi - 1);
	_gsi_base_sel = (hip->mem_desc_offset - hip->cpu_desc_offset) / hip->cpu_desc_size;

	if (verbose_boot_info) {
		printf(":virt_alloc: "); _core_mem_alloc.virt_alloc()->raw()->dump_addr_tree();
		printf(":phys_alloc: "); _core_mem_alloc.phys_alloc()->raw()->dump_addr_tree();
		printf(":io_mem_alloc: "); _io_mem_alloc.raw()->dump_addr_tree();
	}

	/* add capability selector ranges to map */
	unsigned index = 0x2000;
	for (unsigned i = 0; i < 16; i++)
	{
		void * phys_ptr = 0;
		ram_alloc()->alloc(4096, &phys_ptr);

		addr_t phys_addr = reinterpret_cast<addr_t>(phys_ptr);
		addr_t core_local_addr = _map_page(phys_addr >> get_page_size_log2(),
		                                   1, false);
		
		Cap_range * range = reinterpret_cast<Cap_range *>(core_local_addr);
		*range = Cap_range(index);

		cap_map()->insert(range);

/*
		if (verbose_boot_info)
			printf("add cap range [0x%8lx:0x%8lx) - physical 0x%8lx -> 0x%8lx\n",
			       range->base(),
			       range->base() + range->elements(), phys_addr, core_local_addr);
*/

		index = range->base() + range->elements();
	}
}


/****************************************
 ** Support for core memory management **
 ****************************************/

bool Core_mem_allocator::_map_local(addr_t virt_addr, addr_t phys_addr,
                                    unsigned size)
{
	map_local((Utcb *)Thread_base::myself()->utcb(), phys_addr,
	          virt_addr, size / get_page_size(),
	          Rights(true, true, true), true);
	return true;
}


bool Core_mem_allocator::_unmap_local(addr_t virt_addr, unsigned size)
{
	unmap_local((Utcb *)Thread_base::myself()->utcb(),
	            virt_addr, size / get_page_size());
	return true;
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit() { sleep_forever(); }


void Core_parent::exit(int exit_value) { }
