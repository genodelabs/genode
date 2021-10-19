/*
 * \brief  'Capture' part of black hole component
 * \author Christian Prochaska
 * \date   2021-09-24
 *
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CAPTURE_H_
#define _CAPTURE_H_

/* Genode includes */
#include <base/session_object.h>
#include <capture_session/capture_session.h>

#include <audio_out_session/rpc_object.h>
#include <timer_session/connection.h>


namespace Capture
{
	using namespace Genode;
	class Session_component;
	class Root;
}


class Capture::Session_component : public Session_object<Capture::Session>
{
	private:

		Env &_env;

		Constrained_ram_allocator _ram;

		Constructible<Attached_ram_dataspace> _buffer { };

	public:

		Session_component(Env              &env,
		                  Resources  const &resources,
		                  Label      const &label,
		                  Diag       const &diag)
		:
			Session_object(env.ep(), resources, label, diag),
			_env(env),
			_ram(env.ram(), _ram_quota_guard(), _cap_quota_guard())
		{ }

		~Session_component() { }


		/*******************************
		 ** Capture session interface **
		 *******************************/

		Area screen_size() const override
		{
			return Area(640, 480);
		}

		void screen_size_sigh(Signal_context_capability) override { }

		void buffer(Area size) override
		{
			if (size.count() == 0) {
				_buffer.destruct();
				return;
			}

			_buffer.construct(_ram, _env.rm(), buffer_bytes(size));
		}

		Dataspace_capability dataspace() override
		{
			if (_buffer.constructed())
				return _buffer->cap();

			return Dataspace_capability();
		}

		Affected_rects capture_at(Point) override
		{
			return Affected_rects();
		}
};


namespace Capture {
	typedef Genode::Root_component<Session_component, Genode::Multiple_clients> Root_component;
}


class Capture::Root : public Capture::Root_component
{
	private:

		Genode::Env &_env;

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t session_size = align_addr(sizeof(Session_component), 12);

			if ((ram_quota < session_size)) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", session_size);
				throw Insufficient_ram_quota();
			}

			Session_component *session = new (md_alloc())
				Session_component(_env,
				                  session_resources_from_args(args),
				                  session_label_from_args(args),
				                  session_diag_from_args(args));

			return session;

		}

		void _upgrade_session(Session_component *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Session_component *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

	public:

		Root(Genode::Env        &env,
		     Genode::Allocator  &md_alloc)
		: Root_component(env.ep(), md_alloc),
		  _env(env) { }
};

#endif /* _CAPTURE_H_ */
