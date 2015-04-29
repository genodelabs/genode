/*
 * \brief  Hardware-resource access
 * \author Christian Helmuth
 * \date   2008-10-21
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/lock.h>
#include <base/stdint.h>
#include <util/avl_tree.h>

#include <io_port_session/connection.h>
#include <io_mem_session/connection.h>

#include <dataspace/client.h>

extern "C" {
#include <dde_kit/resources.h>
#include <dde_kit/pgtab.h>
}

#include "device.h"

using namespace Genode;

static const bool verbose = false;

class Range : public Avl_node<Range>
{
	public:

		class Not_found : public Exception { };
		class Overlap : public Exception { };
		class Resource_not_accessible : public Exception { };

	private:

		addr_t _base;
		size_t _size;

	public:

		Range(addr_t base, size_t size) : _base(base), _size(size) { }

		/** AVL node comparison */
		bool higher(Range *range) {
			return (_base + _size <= range->_base); }

		/** AVL node lookup */
		Range *lookup(addr_t addr, size_t size)
		{
			Range *r = this;

			do {
				if (addr >= r->_base) {
					if (addr + size <= r->_base + r->_size)
						return r;
					else if (addr < r->_base + r->_size)
						throw Overlap();
				}

				if (addr < r->_base)
					r = r->child(LEFT);
				else
					r = r->child(RIGHT);
			} while (r);

			throw Not_found();
		}

		/**
		 * Log ranges of node and all children
		 */
		void log_ranges()
		{
			if (child(LEFT))
				child(LEFT)->log_ranges();

			PLOG("  [%08lx,%08lx)", _base, _base + _size);

			if (child(RIGHT))
				child(RIGHT)->log_ranges();
		}

		addr_t base() const { return _base; }
		size_t size() const { return _size; }
};


template <typename TYPE>
class Range_database :  Avl_tree<Range>
{
	private:

		Lock _lock;

		void _log_ranges(const char *op, addr_t b, size_t s)
		{
			PLOG("Range_db %p: %s [%08lx,%08lx)", this, op, b, b + s);
			if (!first())
				PLOG("  <no ranges>");
			else
				first()->log_ranges();
		}

	public:

		TYPE *lookup(addr_t addr, size_t size)
		{
			Lock::Guard lock_guard(_lock);

			if (!first()) throw Range::Not_found();

			return static_cast<TYPE *>(first()->lookup(addr, size));
		}

		void insert(Range *range)
		{
			Lock::Guard lock_guard(_lock);

			Avl_tree<Range>::insert(range);

			if (verbose)
				_log_ranges("INSERT", range->base(), range->size());
		}

		void remove(Range *range)
		{
			Lock::Guard lock_guard(_lock);

			Avl_tree<Range>::remove(range);

			if (verbose)
				_log_ranges("REMOVE", range->base(), range->size());
		}
};


/***************
 ** I/O ports **
 ***************/

class Port_range;
static Range_database<Port_range> *ports()
{
	static Range_database<Port_range> _ports;
	return &_ports;
}


class Port_range : public Range, public Io_port_session_client
{
	public:

		Port_range(addr_t base, size_t size, Io_port_session_capability cap)
		: Range(base, size), Io_port_session_client(cap) {
			ports()->insert(this); }

		~Port_range() { ports()->remove(this); }
};


extern "C" int dde_kit_request_io(dde_kit_addr_t addr, dde_kit_size_t size,
                                  unsigned short bar, dde_kit_uint8_t bus,
                                  dde_kit_uint8_t dev, dde_kit_uint8_t func)
{
	try {

		new (env()->heap()) Port_range(addr, size, Dde_kit::Device::io_port(bus, dev, func, bar));

		return 0;
	} catch (...) {
		return -1;
	}
}


extern "C" int dde_kit_release_io(dde_kit_addr_t addr, dde_kit_size_t size)
{
	try {
		destroy(env()->heap(), ports()->lookup(addr, size));

		return 0;
	} catch (...) {
		return -1;
	}
}


extern "C" unsigned char dde_kit_inb(dde_kit_addr_t port)
{
	try {
		return ports()->lookup(port, 1)->inb(port);
	} catch (...) {
		return 0;
	}
}


extern "C" unsigned short dde_kit_inw(dde_kit_addr_t port)
{
	try {
		return ports()->lookup(port, 2)->inw(port);
	} catch (...) {
		return 0;
	}
}


extern "C" unsigned long dde_kit_inl(dde_kit_addr_t port)
{
	try {
		return ports()->lookup(port, 4)->inl(port);
	} catch (...) {
		return 0;
	}
}


extern "C" void dde_kit_outb(dde_kit_addr_t port, unsigned char val)
{
	try {
		ports()->lookup(port, 1)->outb(port, val);
	} catch (...) { }
}


extern "C" void dde_kit_outw(dde_kit_addr_t port, unsigned short val)
{
	try {
		ports()->lookup(port, 2)->outw(port, val);
	} catch (...) { }
}


extern "C" void dde_kit_outl(dde_kit_addr_t port, unsigned long val)
{
	try {
		ports()->lookup(port, 4)->outl(port, val);
	} catch (...) { }
}


/******************
 ** MMIO regions **
 ******************/

class Mem_range;
static Range_database<Mem_range> *mem_db()
{
	static Range_database<Mem_range> _mem_db;
	return &_mem_db;
}


class Mem_range : public Range, public Io_mem_connection
{
	private:

		bool                        _wc;

		Io_mem_dataspace_capability _ds;

		addr_t                      _vaddr;

	public:

		Mem_range(addr_t base, size_t size, bool wc)
		:
			Range(base, size), Io_mem_connection(base, size, wc),
			_wc(wc), _ds(dataspace())
		{
			if (!_ds.valid()) throw Resource_not_accessible();

			_vaddr  = env()->rm_session()->attach(_ds);
			_vaddr |= base & 0xfff;

			dde_kit_pgtab_set_region_with_size((void *)_vaddr, base, size);
			mem_db()->insert(this);
		}

		~Mem_range()
		{
			mem_db()->remove(this);
			dde_kit_pgtab_clear_region((void *)_vaddr);
		}

		addr_t vaddr()     const { return _vaddr; }
		bool   wc()        const { return _wc; }
};


extern "C" int dde_kit_request_mem(dde_kit_addr_t addr, dde_kit_size_t size,
                                   int wc, dde_kit_addr_t *vaddr)
{
	/*
	 * We check if a resource comprising the requested region was allocated
	 * before (with the same access type, i.e., wc flag) and then return the
	 * mapping address. In case of overlapping requests, there's nothing else
	 * for it but to return an error.
	 */
	try {
		Mem_range *r = mem_db()->lookup(addr, size);

		if (!!wc != r->wc()) {
			PERR("I/O memory access type mismatch");
			return -1;
		}

		*vaddr = r->vaddr() + (addr - r->base());

		return 0;
	} catch (Mem_range::Overlap) {
		PERR("overlapping I/O memory region requested");
		return -1;
	} catch (Mem_range::Not_found) { }

	/* request resource if no previous allocation was found */
	try {
		*vaddr = (new (env()->heap()) Mem_range(addr, size, !!wc))->vaddr();

		return 0;
	} catch (...) {
		return -1;
	}
}


extern "C" int dde_kit_release_mem(dde_kit_addr_t addr, dde_kit_size_t size)
{
	try {
		destroy(env()->heap(), mem_db()->lookup(addr, size));

		return 0;
	} catch (...) {
		return -1;
	}
}
