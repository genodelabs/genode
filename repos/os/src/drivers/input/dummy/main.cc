/**
 * \brief  Input driver front-end
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <base/sleep.h>
#include <base/signal.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <input_session/input_session.h>
#include <input/event.h>
#include <base/printf.h>

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

			bool is_pending() const override { return 0; }

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

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc)
			: Root_component<Session_component>(session_ep, md_alloc) { }
	};
}


int main(int argc, char **argv)
{
	/* create dataspace for event buffer that is shared with the client */
	try { ev_ds_cap = env()->ram_session()->alloc(MAX_EVENTS*sizeof(Input::Event)); }
	catch (Ram_session::Alloc_failed) {
		PERR("Could not allocate dataspace for event buffer");
		return 1;
	}

	/* initialize server entry point */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "input_ep");

	/* entry point serving input root interface */
	static Input::Root input_root(&ep, env()->heap());

	/* tell parent about the service */
	env()->parent()->announce(ep.manage(&input_root));

	/* main's done - go to sleep */

	sleep_forever();
	return 0;
}
