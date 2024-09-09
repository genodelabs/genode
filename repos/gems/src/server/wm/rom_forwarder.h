/*
 * \brief  ROM service for intercepting clipboard ROMs
 * \author Norman Feske
 * \date   2019-06-26
 *
 * This ROM service can be used as proxy for clipboard ROMs to ensure the
 * consistency of the client labels appearing at the clipboard component
 * with the label of the currently focused GUI client.
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ROM_FORWARDER_H_
#define _ROM_FORWARDER_H_

/* Genode includes */
#include <root/component.h>
#include <rom_session/connection.h>

namespace Wm { struct Rom_forwarder; }


struct Wm::Rom_forwarder
{
	struct Session : Session_object<Rom_session>
	{
		Rom_connection _connection;

		Session(Env &env, auto &&... args)
		:
			Session_object<Rom_session>(env.ep(), args...),
			_connection(env, label())
		{ }

		void upgrade(Session::Resources const &resources)
		{
			_connection.upgrade(resources);
		}


		/***************************
		 ** Rom_session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override
		{
			return _connection.dataspace();
		}

		bool update() override
		{
			return _connection.update();
		}

		void sigh(Signal_context_capability sigh) override
		{
			_connection.sigh(sigh);
		}
	};

	struct Root : Root_component<Session>
	{
		Env       &_env;
		Allocator &_alloc;

		Session *_create_session(char const *args) override
		{
			return new (md_alloc()) Session(_env, session_resources_from_args(args),
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

	Rom_forwarder(Env &env, Allocator &alloc) : _root(env, alloc) { }
};

#endif /* _ROM_FORWARDER_H_ */
