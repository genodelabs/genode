/*
 * \brief  Report service for intercepting shape and clipboard reports
 * \author Norman Feske
 * \date   2019-02-18
 *
 * This report service has the sole purpose of applying the same labeling
 * policy to an application's shape report as done for the application's
 * 'Nitpicker' session. This consistency is needed by the pointer to correlate
 * the currently hovered nitpicker session with the reported shapes.
 * Analogously, clipboard reports can be routed through the window
 * manager to support the clipboard component with associating its clients
 * with nitpicker's reported focus.
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REPORT_FORWARDER_H_
#define _REPORT_FORWARDER_H_

/* Genode includes */
#include <root/component.h>
#include <report_session/connection.h>

namespace Wm { struct Report_forwarder; }


struct Wm::Report_forwarder
{
	struct Session : Genode::Rpc_object<Report::Session>
	{
		Genode::Env &_env;
		Report::Connection _connection;

		Session(Genode::Env &env, Genode::Session_label const &label,
		        size_t buffer_size)
		: _env(env), _connection(env, label.string(), buffer_size)
		{ _env.ep().manage(*this); }

		~Session() { _env.ep().dissolve(*this); }

		void upgrade(Genode::Session::Resources const &resources)
		{
			_connection.upgrade(resources);
		}


		/*******************************
		 ** Report::Session interface **
		 *******************************/

		Genode::Dataspace_capability dataspace() override
		{
			return _connection.dataspace();
		}

		void submit(Genode::size_t length) override
		{
			_connection.submit(length);
		}

		void response_sigh(Genode::Signal_context_capability sigh) override
		{
			_connection.response_sigh(sigh);
		}

		Genode::size_t obtain_response() override
		{
			return _connection.obtain_response();
		}
	};

	struct Root : Genode::Root_component<Session>
	{
		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Session *_create_session(char const *args) override
		{
			return new (md_alloc())
				Session(_env, Genode::label_from_args(args),
			            Arg_string::find_arg(args, "buffer_size").ulong_value(0));
		}

		void _upgrade_session(Session *session, const char *args) override
		{
			session->upgrade(Genode::session_resources_from_args(args));
		}

		Root(Genode::Env &env, Genode::Allocator &alloc)
		:
			Genode::Root_component<Session>(env.ep(), alloc),
			_env(env), _alloc(alloc)
		{
			_env.parent().announce(env.ep().manage(*this));
		}

	} _root;

	Report_forwarder(Genode::Env &env, Genode::Allocator &alloc)
	: _root(env, alloc) { }
};

#endif /* _REPORT_FORWARDER_H_ */
