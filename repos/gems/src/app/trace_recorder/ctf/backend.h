/*
 * \brief  CTF backend
 * \author Johannes Schlatow
 * \date   2021-08-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__BACKEND_H_
#define _CTF__BACKEND_H_

/* local includes */
#include <ctf/metadata.h>
#include <ctf/write_buffer.h>
#include <backend.h>
#include <timestamp_calibrator.h>

#include <os/vfs.h>
#include <base/attached_rom_dataspace.h>

namespace Ctf {
	using namespace Trace_recorder;

	using Genode::Directory;
	using Genode::New_file;

	using Buffer = Write_buffer<32*1024>;

	class Backend;
	class Writer;
}


class Ctf::Writer : public Trace_recorder::Writer_base
{
	private:
		Buffer                  &_packet_buffer;
		Constructible<New_file>  _dst_file      { };
		Directory::Path          _file_path     { };

	public:
		Writer(Genode::Registry<Writer_base> &registry, Buffer &packet_buffer)
		: Writer_base(registry),
		  _packet_buffer(packet_buffer)
		{ }

		virtual void start_iteration(Directory &,
		                             Directory::Path const &,
		                             ::Subject_info const &) override;

		virtual void process_event(Trace_recorder::Trace_event_base const &, Genode::size_t) override;

		virtual void end_iteration() override;
};


class Ctf::Backend : Trace_recorder::Backend_base
{
	private:

		Attached_rom_dataspace      _metadata_rom;
		Metadata                    _metadata;

		Buffer                      _packet_buf      { };

	public:

		Backend(Env &env, Timestamp_calibrator const &ts_calibrator, Backends &backends)
		: Backend_base(backends, "ctf"),
		  _metadata_rom(env, "metadata"),
		  _metadata(_metadata_rom, ts_calibrator.ticks_per_second())
		{ }

		Writer_base &create_writer(Genode::Allocator             &alloc,
		                           Genode::Registry<Writer_base> &registry,
		                           Directory                     &root,
		                           Directory::Path        const  &path) override
		{
			/* copy metadata file while adapting clock declaration */
			Directory::Path metadata_path { Directory::join(path, "metadata") };
			if (!root.file_exists(metadata_path)) {
				New_file        metadata_file { root, metadata_path };
				_metadata.write_file(metadata_file);
			}

			return *new (alloc) Writer(registry, _packet_buf);
		}
};


#endif /* _CTF__BACKEND_H_ */
