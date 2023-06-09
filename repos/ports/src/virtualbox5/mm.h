/*
 * \brief  VirtualBox memory manager (MMR3)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <rm_session/rm_session.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

#include <util/retry.h>

/**
 * Sub rm_session as backend for the Libc::Mem_alloc implementation.
 * Purpose is that memory allocation by vbox of a specific type (MMTYP) are
 * all located within a 2G virtual window. Reason is that virtualbox converts
 * internally pointers at several places in base + offset, whereby offset is
 * a int32_t type.
 */
class Sub_rm_connection : private Genode::Rm_connection,
                          public Genode::Region_map_client
{

	private:

		Genode::addr_t const _offset;
		Genode::size_t const _size;

	public:

		Sub_rm_connection(Genode::Env &env, Genode::size_t size)
		:
			Rm_connection(env),
			Genode::Region_map_client(Rm_connection::create(size)),
			_offset(env.rm().attach(dataspace(), 0, 0, false, nullptr, true, true)),
			_size(size)
		{ }

		Local_addr attach(Genode::Dataspace_capability ds,
		                  Genode::size_t size = 0, Genode::off_t offset = 0,
		                  bool use_local_addr = false,
		                  Local_addr local_addr = (void *)0,
		                  bool executable = false,
		                  bool writeable = true) override
		{
			Local_addr addr = Genode::retry<Genode::Out_of_ram>(
				[&] () {
					return Genode::retry<Genode::Out_of_caps>(
						[&] () {
							return Region_map_client::attach(ds, size, offset,
							                                 use_local_addr,
							                                 local_addr,
							                                 executable,
							                                 writeable); },
						[&] () { upgrade_caps(2); });
					},
				[&] () { upgrade_ram(8192); });

			Genode::addr_t new_addr = addr;
			new_addr += _offset;
			return Local_addr(new_addr);
		}

		bool contains(void * ptr) const
		{
			Genode::addr_t addr = reinterpret_cast<Genode::addr_t>(ptr);

			return (_offset <= addr && addr < _offset + _size);
		}

		bool contains(Genode::addr_t addr) const {
			return (_offset <= addr && addr < _offset + _size); }

		Genode::addr_t local_addr(Genode::addr_t addr) const {
			return _offset + addr; }
};
