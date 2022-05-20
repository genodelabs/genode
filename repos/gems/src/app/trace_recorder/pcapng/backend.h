/*
 * \brief  PCAPNG backend
 * \author Johannes Schlatow
 * \date   2022-05-13
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__BACKEND_H_
#define _PCAPNG__BACKEND_H_

/* local includes */
#include <backend.h>
#include <pcapng/write_buffer.h>
#include <pcapng/interface_registry.h>
#include <timestamp_calibrator.h>

/* Genode includes */
#include <os/vfs.h>

namespace Pcapng {
	using namespace Trace_recorder;

	using Genode::Directory;
	using Genode::New_file;

	using Buffer = Write_buffer<32*1024>;

	class Backend;
	class Writer;
}


class Pcapng::Writer : public Trace_recorder::Writer_base
{
	private:
		Interface_registry         &_interface_registry;
		Buffer                     &_buffer;
		Timestamp_calibrator const &_ts_calibrator;
		Constructible<New_file>     _dst_file      { };
		Directory::Path             _file_path     { };
		bool                        _empty_section { false };

	public:
		Writer(Genode::Registry<Writer_base> &registry, Interface_registry &interface_registry, Buffer &buffer, Timestamp_calibrator const &ts_calibrator)
		: Writer_base(registry),
		  _interface_registry(interface_registry),
		  _buffer(buffer),
		  _ts_calibrator(ts_calibrator)
		{ }

		virtual void start_iteration(Directory &,
		                             Directory::Path const &,
		                             ::Subject_info const &) override;

		virtual void process_event(Trace_recorder::Trace_event_base const &, Genode::size_t) override;

		virtual void end_iteration() override;
};


class Pcapng::Backend : Trace_recorder::Backend_base
{
	private:

		Interface_registry          _interface_registry;
		Buffer                      _buffer { };
		Timestamp_calibrator const &_ts_calibrator;

	public:

		Backend(Allocator &alloc, Timestamp_calibrator const &ts_calibrator, Backends &backends)
		: Backend_base(backends, "pcapng"),
		  _interface_registry(alloc),
		  _ts_calibrator(ts_calibrator)
		{ }

		Writer_base &create_writer(Genode::Allocator             &,
		                           Genode::Registry<Writer_base> &,
		                           Directory                     &,
		                           Directory::Path      const    &) override;
};


#endif /* _PCAPNG__BACKEND_H_ */
