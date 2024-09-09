/*
 * \brief  Report service for intercepting shape and clipboard reports
 * \author Norman Feske
 * \date   2019-02-18
 *
 * This report service has the sole purpose of applying the same labeling
 * policy to an application's shape report as done for the application's GUI
 * session. This consistency is needed by the pointer to correlate the
 * currently hovered GUI session with the reported shapes.
 *
 * Analogously, clipboard reports can be routed through the window manager to
 * support the clipboard component with associating its clients with
 * nitpicker's reported focus.
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
	struct Session : Session_object<Report::Session>
	{
		Report::Connection _connection;

		Session(Env &env, size_t buffer_size, auto &&... args)
		:
			Session_object<Report::Session>(env.ep(), args...),
			_connection(env, label(), buffer_size)
		{ }

		void upgrade(Session::Resources const &resources)
		{
			_connection.upgrade(resources);
		}


		/*******************************
		 ** Report::Session interface **
		 *******************************/

		Dataspace_capability dataspace() override
		{
			return _connection.dataspace();
		}

		void submit(size_t length) override
		{
			_connection.submit(length);
		}

		void response_sigh(Signal_context_capability sigh) override
		{
			_connection.response_sigh(sigh);
		}

		size_t obtain_response() override
		{
			return _connection.obtain_response();
		}
	};

	struct Root : Root_component<Session>
	{
		Env       &_env;
		Allocator &_alloc;

		Session *_create_session(char const *args) override
		{
			return new (md_alloc())
				Session(_env, Arg_string::find_arg(args, "buffer_size").ulong_value(0),
			                  session_resources_from_args(args),
			                  session_label_from_args(args),
			                  session_diag_from_args(args));
		}

		void _upgrade_session(Session *session, const char *args) override
		{
			session->upgrade(session_resources_from_args(args));
		}

		Root(Env &env, Allocator &alloc)
		:
			Root_component<Session>(env.ep(), alloc),
			_env(env), _alloc(alloc)
		{
			_env.parent().announce(env.ep().manage(*this));
		}

	} _root;

	Report_forwarder(Env &env, Allocator &alloc) : _root(env, alloc) { }
};

#endif /* _REPORT_FORWARDER_H_ */
