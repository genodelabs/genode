/*
 * \brief  Provide sync signals for cross-component synchronization
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <root/component.h>

/* local includes */
#include <sync_session/connection.h>

namespace Sync
{
	enum { NR_OF_SIGNALS = 1 };

	using Server::Entrypoint;
	using Genode::Rpc_object;
	using Genode::env;
	using Genode::Root_component;
	using Genode::Allocator;
	using Genode::Signal_transmitter;

	class  Signal;
	class  Session_component;
	class  Root;
	struct Main;
}

class Sync::Signal
{
	friend class Root;

	private:

		enum { NR_OF_TRANSMITTERS = 9 };

		Signal_transmitter _transmitters[NR_OF_TRANSMITTERS];
		unsigned           _submitted;
		unsigned           _threshold;

		void _check()
		{
			if (_submitted < _threshold) { return; }
			for (unsigned i = 0; i < _submitted; i++) {
				_transmitters[i].submit(); }
			_submitted = 0;
		}

		void _reset()
		{
			_submitted = 0;
			_threshold = 0;
		}

	public:

		void threshold(unsigned const threshold)
		{
			_threshold = threshold;
			_check();
		}

		void submit(Signal_context_capability & sigc)
		{
			_transmitters[_submitted] = Signal_transmitter(sigc);
			_submitted++;
			_check();
		}
};

class Sync::Session_component : public Rpc_object<Session>
{
	private:

		Signal * const _signals;

	public:

		Session_component(Signal * const signals) : _signals(signals) { }

		void threshold(unsigned id, unsigned threshold) override
		{
			if (id >= NR_OF_SIGNALS) { return; }
			_signals[id].threshold(threshold);
		}

		void
		submit(unsigned const id, Signal_context_capability sigc) override
		{
			if (id >= NR_OF_SIGNALS) { return; }
			_signals[id].submit(sigc);
		}
};

class Sync::Root : public Root_component<Session_component>
{
	private:

		Signal _signals[NR_OF_SIGNALS];

	protected:

		Session_component *_create_session(const char *args)
		{
			try { return new (md_alloc()) Session_component(_signals); }
			catch (...) { throw Root::Exception(); }
		}

	public:

		Root(Entrypoint & ep, Allocator & md_alloc)
		: Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{
			for (unsigned i = 0; i < NR_OF_SIGNALS; i++) {
				_signals[i]._reset(); }
		}
};

struct Sync::Main
{
	Server::Entrypoint & ep;

	Root root;

	Main(Server::Entrypoint & ep) : ep(ep), root(ep, *env()->heap()) {
		env()->parent()->announce(ep.manage(root)); }
};


/************
 ** Server **
 ************/

namespace Server
{
	using namespace Sync;

	char const *name() { return "sync_ep"; }

	size_t stack_size() { return 2 * 1024 * sizeof(long); }

	void construct(Entrypoint & ep) { static Main main(ep); }
}
