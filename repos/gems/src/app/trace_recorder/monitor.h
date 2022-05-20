/*
 * \brief  Frontend for controlling the TRACE session
 * \author Johannes Schlatow
 * \date   2022-05-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR_H_
#define _MONITOR_H_

/* local includes */
#include <subject_info.h>
#include <policy.h>
#include <backend.h>
#include <ctf/backend.h>
#include <pcapng/backend.h>
#include <timestamp_calibrator.h>

/* Genode includes */
#include <base/registry.h>
#include <os/session_policy.h>
#include <os/vfs.h>
#include <trace/trace_buffer.h>
#include <rtc_session/connection.h>

namespace Trace_recorder {
	using namespace Genode;

	class Monitor;
}

class Trace_recorder::Monitor
{
	private:
		enum { DEFAULT_BUFFER_SIZE      = 64 * 1024 };
		enum { TRACE_SESSION_RAM        = 1024 * 1024 };
		enum { TRACE_SESSION_ARG_BUFFER = 128 * 1024 };

		class Trace_directory
		{
			private:
				Root_directory  _root;
				Directory::Path _path;

			public:

				static Directory::Path root_from_config(Xml_node &config) {
					return config.attribute_value("target_root", Directory::Path("/")); }

				Trace_directory(Env             &env,
				                Allocator       &alloc,
				                Xml_node        &config,
				                Rtc::Connection &rtc)
				: _root(env, alloc, config.sub_node("vfs")),
				  _path(Directory::join(root_from_config(config), rtc.current_time()))
				{ };

				Directory       &root()        { return _root; }
				Directory::Path  subject_path(::Subject_info const &info);
		};

		class Attached_buffer
		{
			private:

				Env                               &_env;
				Trace_buffer                       _buffer;
				Registry<Attached_buffer>::Element _element;
				Subject_info                       _info;
				Trace::Subject_id                  _subject_id;
				Registry<Writer_base>              _writers { };

			public:

				Attached_buffer(Registry<Attached_buffer>    &registry,
				                Genode::Env                  &env,
				                Genode::Dataspace_capability ds,
				                Trace::Subject_info    const &info,
				                Trace::Subject_id            id)
				: _env(env),
				  _buffer(*((Trace::Buffer*)_env.rm().attach(ds))),
				  _element(registry, *this),
				  _info(info),
				  _subject_id(id)
				{ }

				~Attached_buffer()
				{
					_env.rm().detach(_buffer.address());
				}

				void process_events(Trace_directory &);

				Registry<Writer_base>   &writers()            { return _writers; }

				Subject_info      const &info()         const { return _info;   }
				Trace::Subject_id const  subject_id()   const { return _subject_id; }
		};

		Env                           &_env;
		Allocator                     &_alloc;
		Registry<Attached_buffer>      _trace_buffers    { };
		Policies                       _policies         { };
		Backends                       _backends         { };
		Constructible<Trace_directory> _trace_directory  { };

		Rtc::Connection                _rtc              { _env };
		Timer::Connection              _timer            { _env };
		Trace::Connection              _trace            { _env,
		                                                   TRACE_SESSION_RAM,
		                                                   TRACE_SESSION_ARG_BUFFER,
		                                                   0 };

		Signal_handler<Monitor>        _timeout_handler  { _env.ep(),
		                                                   *this,
		                                                   &Monitor::_handle_timeout };

		Timestamp_calibrator           _ts_calibrator    { _env, _rtc, _timer };

		/* built-in backends */
		Ctf::Backend                   _ctf_backend      { _env,   _ts_calibrator, _backends };
		Pcapng::Backend                _pcapng_backend   { _alloc, _ts_calibrator, _backends };

		/* methods */
		Session_policy _session_policy(Trace::Subject_info const &info, Xml_node config);
		void           _handle_timeout();

	public:

		Monitor(Env &env, Allocator &alloc)
		: _env(env),
		  _alloc(alloc)
		{
			_timer.sigh(_timeout_handler);
		}

		void start(Xml_node config);
		void stop();
};


#endif /* _MONITOR_H_ */
