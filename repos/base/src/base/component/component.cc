/*
 * \brief  Component bootstrap
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/env.h>


namespace {

	struct Env : Genode::Env
	{
		Genode::Entrypoint &_ep;

		Env(Genode::Entrypoint &ep) : _ep(ep) { }

		Genode::Parent      &parent() override { return *Genode::env()->parent(); }
		Genode::Ram_session &ram()    override { return *Genode::env()->ram_session(); }
		Genode::Cpu_session &cpu()    override { return *Genode::env()->cpu_session(); }
		Genode::Region_map  &rm()     override { return *Genode::env()->rm_session(); }
		Genode::Pd_session  &pd()     override { return *Genode::env()->pd_session(); }
		Genode::Entrypoint  &ep()     override { return _ep; }

		Genode::Ram_session_capability ram_session_cap() override
		{
			return Genode::env()->ram_session_cap();
		}

		Genode::Cpu_session_capability cpu_session_cap() override
		{
			return Genode::env()->cpu_session_cap();
		}
	};
}


namespace Genode {

	struct Startup;

	extern void bootstrap_component();
}


/*
 * We need to execute the constructor of the main entrypoint from a
 * class called 'Startup' as 'Startup' is a friend of 'Entrypoint'.
 */
struct Genode::Startup
{
	::Env env { ep };

	/*
	 * The construction of the main entrypoint does never return.
	 */
	Entrypoint ep { env };
};


void Genode::bootstrap_component()
{
	static Startup startup;

	/* never reached */
}
