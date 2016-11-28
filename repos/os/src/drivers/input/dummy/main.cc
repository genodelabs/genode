/**
 * \brief  Input driver front-end
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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


/*****************************************
 ** Implementation of the input service **
 *****************************************/

/*
 * Input event buffer that is shared with the client
 */
enum { MAX_EVENTS = 1000 };
static Dataspace_capability ev_ds_cap;

namespace Input {

	class Session_component : public Genode::Rpc_object<Session>
	{
		public:

			Dataspace_capability dataspace() override { return ev_ds_cap; }

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

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(); }

		public:

			Root(Genode::Entrypoint &ep, Allocator &md_alloc)
			: Root_component<Session_component>(ep, md_alloc) { }
	};
}


struct Main
{
	Genode::Sliced_heap heap;

	Input::Root root;

	Main(Genode::Env &env)
	:
		heap(env.ram(), env.rm()),
		root(env.ep(), heap)
	{
		/* create dataspace for event buffer that is shared with the client */
		try { ev_ds_cap = env.ram().alloc(MAX_EVENTS*sizeof(Input::Event)); }
		catch (Ram_session::Alloc_failed) {
			Genode::error("could not allocate dataspace for event buffer");
			throw Genode::Exception();
		}

		/* tell parent about the service */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main server(env); }
