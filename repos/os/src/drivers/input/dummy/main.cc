/**
 * \brief  Input driver front-end
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/env.h>
#include <base/signal.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <input_session/input_session.h>
#include <input/event.h>
#include <base/log.h>

using namespace Genode;


namespace Input {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Dataspace_capability _ev_ds_cap;

		public:

			Session_component(Dataspace_capability ev_ds_cap)
			: _ev_ds_cap(ev_ds_cap) { }

			Dataspace_capability dataspace() override { return _ev_ds_cap; }

			bool pending() const override { return 0; }

			int flush() override
			{
				/* return number of flushed events */
				return 0;
			}

			void sigh(Genode::Signal_context_capability) override { }
	};


	class Root : public Root_component<Session_component>
	{
		protected:

			Dataspace_capability _ev_ds_cap;

			Session_component *_create_session(const char *) {
				return new (md_alloc()) Session_component(_ev_ds_cap); }

		public:

			Root(Genode::Entrypoint &ep, Allocator &md_alloc,
			     Dataspace_capability ev_ds_cap)
			:
				Root_component<Session_component>(ep, md_alloc),
				_ev_ds_cap(ev_ds_cap)
			{ }
	};
}


struct Main
{
	Genode::Env &env;

	Sliced_heap heap { env.ram(), env.rm() };

	Dataspace_capability ev_ds_cap {
		env.ram().alloc(1000*sizeof(Input::Event)) };

	Input::Root root { env.ep(), heap, ev_ds_cap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main server(env); }
