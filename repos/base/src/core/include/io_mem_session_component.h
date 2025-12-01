/*
 * \brief  Core-specific instance of the IO_MEM session interface
 * \author Christian Helmuth
 * \date   2006-09-14
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <io_mem_session/io_mem_session.h>

/* core includes */
#include <dataspace_component.h>

namespace Core { class Io_mem_session_component; }


class Core::Io_mem_session_component : public Rpc_object<Io_mem_session>
{
	private:

		/*
		 * Helper class used to pass the dataspace attributes as
		 * parameters from the _prepare_io_mem function to the
		 * constructor of Dataspace_component.
		 */
		struct Dataspace_attr
		{
			size_t size            { 0 };
			addr_t core_local_addr { 0 };
			addr_t phys_addr       { 0 };
			Cache  cacheable       { UNCACHED };

			/**
			 * Base address of request used for freeing mem-ranges
			 */
			addr_t req_base { 0 };

			/**
			 * Default constructor
			 *
			 * This constructor enables Dataspace_attr objects to be
			 * returned from the '_prepare_io_mem' function.
			 */
			Dataspace_attr() { }

			/**
			 * Constructor
			 *
			 * An invalid dataspace is represented by setting all
			 * arguments to zero.
			 */
			Dataspace_attr(size_t s, addr_t cla, addr_t pa, Cache c,
			               addr_t req_base)
			:
				size(s), core_local_addr(cla), phys_addr(pa),
				cacheable(c), req_base(req_base) { }
		};

		struct Io_dataspace_component : Dataspace_component
		{
			Io_dataspace_component(Dataspace_attr da)
			:
				Dataspace_component(da.size, da.core_local_addr,
				                    da.phys_addr, da.cacheable,
				                    true, 0) { }

			bool valid() { return size() != 0; }
		};

		struct Phys_range
		{
			addr_t req_base;
			size_t req_size;

			/* page size aligned base */
			addr_t base() const {
				return req_base & ~(get_page_size() - 1); }

			/* page size aligned size */
			size_t size() const
			{
				addr_t end  = align_addr(req_base + req_size, AT_PAGE);
				return end - base();
			}
		};

		Cache _cacheable_attr(char const * args) const
		{
			Arg a = Arg_string::find_arg(args, "wc");
			if (a.valid() && a.bool_value(0))
				return Cache::WRITE_COMBINED;
			else
				return Cache::UNCACHED;
		}

		Phys_range _phys_range(Range_allocator &ram_alloc, const char *args) const
		{
			auto request = Phys_range(Arg_string::find_arg(args, "base").ulong_value(0),
			                          Arg_string::find_arg(args, "size").ulong_value(0));
			auto const req_size = request.req_size;
			auto const req_base = request.req_base;
			auto const size     = request.size();
			auto const base     = request.base();

			/* check for RAM collision */
			if (ram_alloc.remove_range(base, size).failed()) {
				error("I/O memory ", Hex_range<addr_t>(base, size), " "
				      "used by RAM allocator");
				return { };
			}

			/**
			 * _Unfortunate_ workaround for Intel PCH GPIO device.
			 *
			 * The ported i2c_hid driver contains driver code for the "Intel
			 * Tigerlake/Alderlake PCH pinctrl/GPIO" device. Unfortunately, acpica
			 * driver also accesses the same device on Lid open/close via ACPI AML code
			 * of the DSDT table to read out the state of a GPIO pin connected to the
			 * notebook lid. This would fail as I/O memory is handed out only once and
			 * cannot be shared. The workaround disables the region check for the
			 * specified GPIO I/O memory regions and provides both drivers shared
			 * access to the regions.
			 *
			 * This is a preliminary workaround. A general solution should separate the
			 * GPIO driver into a component (e.g., platform driver) that regulates
			 * accesses by i2c_hid and acpica.
			 */
			bool skip_iomem_check = (req_base == 0xfd6d0000ull && req_size == 4096) ||
			                        (req_base == 0xfd6a0000ull && req_size == 4096) ||
			                        (req_base == 0xfd6e0000ull && req_size == 4096);

			/* probe for free region */
			if (!skip_iomem_check &&
			     _io_mem_alloc.alloc_addr(req_size, req_base).failed()) {
				error("I/O memory ", Hex_range<addr_t>(req_base, req_size),
				      " not available");
				return { };
			}

			return request;
		}

		struct Guard : Genode::Noncopyable
		{
			Io_mem_session_component &_io_mem;

			Guard(Io_mem_session_component &iomem) : _io_mem(iomem) { }

			~Guard() {
				if (_io_mem._ds_attr.size) _io_mem._release(_io_mem._ds_attr); }
		};

		Range_allocator            &_io_mem_alloc;
		Cache             const     _cacheable;
		Phys_range        const     _phys_attr;
		Dataspace_attr    const     _ds_attr;
		Guard             const     _release_guard { *this };
		Io_dataspace_component      _ds            { _ds_attr };
		Allocator::Result const     _io_mem_result;
		Rpc_entrypoint             &_ds_ep;
		Io_mem_dataspace_capability _ds_cap    { };

		/********************************************
		 ** Platform-implemented support functions **
		 ********************************************/

		/**
		 * base-<kernel> specific acquire of IO-MEM
		 */
		Dataspace_attr _acquire(Phys_range);

		/**
		 * base-<kernel> specific releasing of IO-MEM
		 */
		void _release(Dataspace_attr const &);

	public:

		/**
		 * Constructor
		 *
		 * \param io_mem_alloc  MMIO region allocator
		 * \param ram_alloc     RAM allocator that will be checked for
		 *                      region collisions
		 * \param ds_ep         entry point to manage the dataspace
		 *                      corresponding the io_mem session
		 * \param args          session construction arguments, in
		 *                      particular MMIO region base, size and
		 *                      caching demands
		 */
		Io_mem_session_component(Range_allocator &io_mem_alloc,
		                         Range_allocator &ram_alloc,
		                         Rpc_entrypoint  &ds_ep,
		                         const char      *args);

		/**
		 * Destructor
		 */
		~Io_mem_session_component();


		/******************************
		 ** Io-mem session interface **
		 ******************************/

		Io_mem_dataspace_capability dataspace() override { return _ds_cap; }
};

#endif /* _CORE__INCLUDE__IO_MEM_SESSION_COMPONENT_H_ */
