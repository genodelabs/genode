/*
 * \brief  x86 emulation binding and support
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_io_mem_dataspace.h>
#include <base/allocator.h>
#include <util/avl_tree.h>
#include <util/misc_math.h>
#include <util/reconstructible.h>
#include <io_port_session/connection.h>

/* format-string includes */
#include <format/snprintf.h>

/* local includes */
#include "ifx86emu.h"
#include "framebuffer.h"
#include "vesa.h"
#include "hw_emul.h"
#include "genode_env.h"


namespace X86emu {
#include"x86emu/x86emu.h"
}

using namespace Vesa;
using namespace Genode;
using X86emu::x86_mem;
using X86emu::PAGESIZE;
using X86emu::CODESIZE;

struct X86emu::X86emu_mem X86emu::x86_mem;

static const bool verbose      = false;
static const bool verbose_mem  = false;
static const bool verbose_port = false;


/***************
 ** Utilities **
 ***************/

class Region : public Avl_node<Region>
{
	private:

		addr_t _base;
		size_t _size;

	public:

		Region(addr_t base, size_t size) : _base(base), _size(size) { }

		/************************
		 ** AVL node interface **
		 ************************/

		bool higher(Region *r) { return r->_base >= _base; }

		/**
		 * Find region the given region fits into
		 */
		Region * match(addr_t base, size_t size)
		{
			Region *r = this;

			do {
				if (base >= r->_base
				 && base + size <= r->_base + r->_size)
					break;

				if (base < r->_base)
					r = r->child(LEFT);
				else
					r = r->child(RIGHT);
			} while (r);

			return r;
		}

		/**
		 * Find region the given region meets
		 *
		 * \return Region overlapping or just meeting (can be merged into
		 *         super region)
		 */
		Region * meet(addr_t base, size_t size)
		{
			Region *r = this;

			do {
				if ((r->_base <= base && r->_base + r->_size >= base)
				 || (base <= r->_base && base + size >= r->_base))
					break;

				if (base < r->_base)
					r = r->child(LEFT);
				else
					r = r->child(RIGHT);
			} while (r);

			return r;
		}

		/**
		 * Log region
		 */
		void print_regions()
		{
			if (child(LEFT))
				child(LEFT)->print_regions();

			log("    ", Hex_range<addr_t>(_base, _size));

			if (child(RIGHT))
				child(RIGHT)->print_regions();
		}

		addr_t base() const { return _base; }
		size_t size() const { return _size; }
};


template <typename TYPE>
struct Region_database : Avl_tree<Region>
{
	Genode::Env       &env;
	Genode::Allocator &heap;

	Region_database(Genode::Env &env, Genode::Allocator &heap)
	: env(env), heap(heap) { }

	TYPE * match(addr_t base, size_t size)
	{
		if (!first()) return 0;

		return static_cast<TYPE *>(first()->match(base, size));
	}

	TYPE * meet(addr_t base, size_t size)
	{
		if (!first()) return 0;

		return static_cast<TYPE *>(first()->meet(base, size));
	}

	TYPE * get_region(addr_t base, size_t size)
	{
		TYPE *region;

		/* look for match and return if found */
		if ((region = match(base, size)))
			return region;

		/*
		 * We try to create a new port region, but first we look if any overlapping
		 * resp. meeting regions already exist. These are freed and merged into a
		 * new super region including the new port region.
		 */

		addr_t beg = base, end = base + size;

		while ((region = meet(beg, end - beg))) {
			/* merge region into super region */
			beg = min(beg, static_cast<addr_t>(region->base()));
			end = max(end, static_cast<addr_t>(region->base() + region->size()));

			/* destroy old region */
			remove(region);
			destroy(heap, region);
		}

		try {
			region = new (heap) TYPE(env, beg, end - beg);
			insert(region);
			return region;
		} catch (...) {
			Genode::error("access to I/O region ",
			              Hex_range<addr_t>(beg, end - beg), " denied");
			return 0;
		}
	}

	void print_regions()
	{
		if (!first()) return;

		first()->print_regions();
	}
};


/**
 * I/O port region including corresponding IO_PORT connection
 */
struct Port_region : Region, Io_port_connection
{
	Port_region(Genode::Env &env, unsigned short port_base, size_t port_size)
	:
		Region(port_base, port_size),
		Io_port_connection(env, port_base, port_size)
	{
		if (verbose) Genode::log("add port ", *this);
	}

	~Port_region()
	{
		if (verbose) Genode::log("del port ", *this);
	}

	void print(Genode::Output &out) const
	{
		unsigned const beg = base(), end = beg + size();
		Genode::print(out, Hex_range<uint16_t>(beg, end - beg));
	}
};


/**
 * I/O memory region including corresponding IO_MEM connection
 */
struct Mem_region : Region, Attached_io_mem_dataspace
{
	Mem_region(Genode::Env &env, addr_t mem_base, size_t mem_size)
	:
		Region(mem_base, mem_size),
		Attached_io_mem_dataspace(env, mem_base, mem_size)
	{
		if (verbose) Genode::log("add mem  ", *this, " @ ", local_addr<void>());
	}

	~Mem_region()
	{
		if (verbose) Genode::log("del mem  ", *this);
	}

	template <typename T>
	T * virt_addr(addr_t addr)
	{
		return reinterpret_cast<T *>(local_addr<char>() + (addr - base()));
	}

	void print(Genode::Output &out) const
	{
		addr_t const beg = base(), end = beg + size();
		Genode::print(out, Hex_range<addr_t>(beg, end - beg));
	}
};


static Genode::Constructible<Region_database<Port_region>> port_region_db;
static Genode::Constructible<Region_database<Mem_region>>  mem_region_db;


/**
 * Setup static memory for x86emu
 */
static int map_code_area(void)
{
	int err;
	Ram_dataspace_capability ds_cap;
	void *dummy;

	/* map page0 */
	if ((err = Framebuffer::map_io_mem(0x0, PAGESIZE, false, &dummy))) {
		Genode::error("could not map page zero");
		return err;
	}
	x86_mem.bios_addr(dummy);

	/* alloc code pages in RAM */
	try {
		static Attached_ram_dataspace ram_ds(genode_env().ram(), genode_env().rm(), CODESIZE);
		dummy = ram_ds.local_addr<void>();
		x86_mem.data_addr(dummy);
	} catch (...) {
		Genode::error("could not allocate dataspace for code");
		return -1;
	}

	/* build opcode command */
	uint32_t code = 0;
	code  = 0xcd;        /* int opcode */
	code |= 0x10 << 8;   /* 10h  */
	code |= 0xf4 << 16;  /* ret opcode */
	memcpy(dummy, &code, sizeof(code));

	return 0;
}


/**********************************
 ** x86emu memory-access support **
 **********************************/

template <typename T>
static T X86API read(X86emu::u32 addr)
{
	/*
	 * Access the last byte of the T value, before actually reading the value.
	 *
	 * If the value of the address is crossing the current region boundary,
	 * the region behind the boundary will be allocated. Both regions will be
	 * merged and can be attached to a different virtual address then when
	 * only accessing the first bytes of the value.
	 */
	T * ret = X86emu::virt_addr<T>(addr + sizeof(T) - 1);
	ret = X86emu::virt_addr<T>(addr);

	if (verbose_mem) {
		unsigned v = *ret;
		Genode::log(" io_mem: read  ",
		            Hex_range<addr_t>(addr, sizeof(T)), ", val=", Hex(v));
	}

	return *ret;
}


template <typename T>
static void X86API write(X86emu::u32 addr, T val)
{
	/* see description of 'read' function */
	X86emu::virt_addr<T>(addr + sizeof(T) - 1);
	*X86emu::virt_addr<T>(addr) = val;

	if (verbose_mem) {
		unsigned v = val;
		Genode::log(" io_mem: write ",
		            Hex_range<addr_t>(addr, sizeof(T)), ", val=", Hex(v));
	}
}


X86emu::X86EMU_memFuncs mem_funcs = {
	&read, &read, &read,
	&write, &write, &write
};


/********************************
 ** x86emu port-access support **
 ********************************/

template <typename T>
static T X86API inx(X86emu::X86EMU_pioAddr addr)
{
	T ret;
	unsigned short port = static_cast<unsigned short>(addr);

	if (hw_emul_handle_port_read(port, &ret))
		return ret;

	Port_region *region = port_region_db->get_region(port, sizeof(T));

	if (!region) return 0;

	switch (sizeof(T)) {
		case 1:
			ret = (T)region->inb(port);
			break;
		case 2:
			ret = (T)region->inw(port);
			break;
		default:
			ret = (T)region->inl(port);
	}

	if (verbose_port) {
		unsigned v = ret;
		Genode::log("io_port: read  ", *region, " value=", Hex(v));
	}

	return ret;
}


template <typename T>
static void X86API outx(X86emu::X86EMU_pioAddr addr, T val)
{
	unsigned short port = static_cast<unsigned short>(addr);

	if (hw_emul_handle_port_write(port, val))
		return;

	Port_region *region = port_region_db->get_region(port, sizeof(T));

	if (!region) return;

	if (verbose_port) {
		unsigned v = val;
		Genode::log("io_port: write ", *region, " value=", Hex(v));
	}

	switch (sizeof(T)) {
		case 1:
			region->outb(port, val);
			break;
		case 2:
			region->outw(port, val);
			break;
		default:
			region->outl(port, val);
	}
}


/**
 * Port access hooks
 */
X86emu::X86EMU_pioFuncs port_funcs = {
	&inx,  &inx,  &inx,
	&outx, &outx, &outx
};


/************************
 ** API implementation **
 ************************/

/* instantiate externally used template funciton */
template char* X86emu::virt_addr<char, uint32_t>(uint32_t addr);
template uint16_t* X86emu::virt_addr<uint16_t, uint32_t>(uint32_t addr);

template <typename TYPE, typename ADDR_TYPE>
TYPE * X86emu::virt_addr(ADDR_TYPE addr)
{
	addr_t      local_addr = static_cast<addr_t>(addr);
	Mem_region *region;

	/* retrieve local mapping for given address */

	/* page 0 */
	if (local_addr >= 0 && local_addr < PAGESIZE)
		local_addr += x86_mem.bios_addr();

	/* fake code segment */
	else if (local_addr >= PAGESIZE && local_addr < (PAGESIZE + CODESIZE))
		local_addr += (x86_mem.data_addr() - PAGESIZE);

	/* any other I/O memory allocated on demand */
	else if ((region = mem_region_db->get_region(addr & ~(PAGESIZE-1), PAGESIZE)))
		return region->virt_addr<TYPE>(local_addr);

	else {
		Genode::warning("invalid address ", Hex(local_addr));
		local_addr = 0;
	}

	return reinterpret_cast<TYPE *>(local_addr);
}


uint16_t X86emu::x86emu_cmd(uint16_t eax, uint16_t ebx, uint16_t ecx,
                            uint16_t edi, uint16_t *out_ebx)
{
	using namespace X86emu;
	M.x86.R_EAX  = eax;          /* int10 function number */
	M.x86.R_EBX  = ebx;
	M.x86.R_ECX  = ecx;
	M.x86.R_EDI  = edi;
	M.x86.R_IP   = 0;            /* address of "int10; ret" */
	M.x86.R_SP   = PAGESIZE;     /* SS:SP pointer to stack */
	M.x86.R_CS   =
	M.x86.R_DS   =
	M.x86.R_ES   =
	M.x86.R_SS   = PAGESIZE >> 4;

	X86EMU_exec();

	if (out_ebx)
	    *out_ebx = M.x86.R_EBX;

	return M.x86.R_AX;
}


void X86emu::print_regions()
{
	log("I/O port regions:");
	port_region_db->print_regions();

	log("I/O memory regions:");
	mem_region_db->print_regions();
}


void X86emu::printk(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	char buf[128];
	Format::String_console sc(buf, sizeof(buf));
	sc.vprintf(format, list);

	va_end(list);
}


void X86emu::init(Genode::Env &env, Allocator &heap)
{
	local_init_genode_env(env, heap);

	port_region_db.construct(env, heap);
	mem_region_db.construct(env, heap);

	if (map_code_area()) throw Framebuffer::Fatal();

	if (verbose) {
		log("--- x86 bios area is ",
		    Hex_range<addr_t>(x86_mem.bios_addr(), PAGESIZE), " ---");
		log("--- x86 data area is ",
		    Hex_range<addr_t>(x86_mem.data_addr(), CODESIZE), " ---");
	}

	X86emu::M.x86.debug = 0;
	X86emu::X86EMU_setupPioFuncs(&port_funcs);
	X86emu::X86EMU_setupMemFuncs(&mem_funcs);
}
