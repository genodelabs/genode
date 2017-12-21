/*
 * \brief  Provide sync signals for cross-component synchronization
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <root/component.h>

/* local includes */
#include <sync_session/connection.h>

using namespace Genode;

class Sync_root;


struct Session_component : public Rpc_object<Sync::Session>
{
	Sync_root &root;

	Session_component(Sync_root &root) : root(root) { }

	void threshold(unsigned threshold)            override;
	void submit(Signal_context_capability signal) override;
};


struct Sync_root : public Root_component<Session_component>
{
	Signal_transmitter transmitters[9];
	unsigned           submitted { 0 };
	unsigned           threshold { 0 };

	void check()
	{
		if (submitted < threshold) { return; }
		for (unsigned i = 0; i < submitted; i++) {
			transmitters[i].submit(); }
		submitted = 0;
	}

	Session_component *_create_session(char const *) override
	{
		try { return new (md_alloc()) Session_component(*this); }
		catch (...) { throw Service_denied(); }
	}

	Sync_root(Entrypoint &ep, Allocator &md_alloc)
	: Root_component<Session_component>(ep, md_alloc) { }
};


void Session_component::threshold(unsigned threshold)
{
	root.threshold = threshold;
	root.check();
}


void Session_component::submit(Signal_context_capability signal)
{
	root.transmitters[root.submitted] = Signal_transmitter(signal);
	root.submitted++;
	root.check();
}


struct Main
{
	Env       &env;
	Heap       heap { env.ram(), env.rm() };
	Sync_root  root { env.ep(), heap };

	Main(Env &env) : env(env) { env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Env &env) { static Main main(env); }
