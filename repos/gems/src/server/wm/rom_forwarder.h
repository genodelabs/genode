/*
 * \brief  ROM service for intercepting clipboard ROMs
 * \author Norman Feske
 * \date   2019-06-26
 *
 * This ROM service can be used as proxy for clipboard ROMs to ensure the
 * consistency of the client labels appearing at the clipboard component
 * with the label of the currently focused nitpicker client.
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
	struct Session : Genode::Rpc_object<Genode::Rom_session>
	{
		Genode::Env &_env;
		Genode::Rom_connection _connection;

		Session(Genode::Env &env, Genode::Session_label const &label)
		: _env(env), _connection(env, label.string())
		{ _env.ep().manage(*this); }

		~Session() { _env.ep().dissolve(*this); }

		void upgrade(Genode::Session::Resources const &resources)
		{
			_connection.upgrade(resources);
		}


		/***************************
		 ** Rom_session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace() override
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

	struct Root : Genode::Root_component<Session>
	{
		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Session *_create_session(char const *args) override
		{
			return new (md_alloc()) Session(_env, Genode::label_from_args(args));
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

	Rom_forwarder(Genode::Env &env, Genode::Allocator &alloc)
	: _root(env, alloc) { }
};

#endif /* _ROM_FORWARDER_H_ */
