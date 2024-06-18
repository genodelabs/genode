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

		Genode::addr_t _attach(Genode::Region_map &local_rm)
		{
			return local_rm.attach(dataspace(), {
				.size       = { },
				.offset     = { },
				.use_at     = { },
				.at         = { },
				.executable = true,
				.writeable  = true
			}).convert<Genode::addr_t>(
				[&] (Range range)  { return range.start; },
				[&] (Attach_error) {
				Genode::error("failed to attach Sub_rm_connection to local address space");
					return 0UL; }
			);
		}

	public:

		Sub_rm_connection(Genode::Env &env, Genode::size_t size)
		:
			Rm_connection(env),
			Genode::Region_map_client(Rm_connection::create(size)),
			_offset(_attach(env.rm())),
			_size(size)
		{ }

		Attach_result attach(Genode::Dataspace_capability ds, Attr const &attr) override
		{
			Attach_result result = Attach_error::REGION_CONFLICT;
			for (;;) {
				result = Region_map_client::attach(ds, attr);
				if      (result == Attach_error::OUT_OF_RAM)  upgrade_ram(8*1024);
				else if (result == Attach_error::OUT_OF_CAPS) upgrade_caps(2);
				else
					break;
			}

			return result.convert<Attach_result>(
				[&] (Range const r)  { return Range { .start = r.start + _offset,
				                                      .num_bytes = r.num_bytes }; },
				[&] (Attach_error e) { return e; });
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
