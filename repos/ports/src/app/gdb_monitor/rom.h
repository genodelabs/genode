/*
 * \brief  ROM service
 * \author Christian Helmuth
 * \date   2011-09-16
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ROM_H_
#define _ROM_H_

/* Genode includes */
#include <base/service.h>
#include <dataspace/client.h>
#include <root/component.h>
#include <rom_session/connection.h>
#include <util/arg_string.h>


namespace Gdb_monitor
{
	class Rom_session_component;
	class Local_rom_factory;
	typedef Genode::Local_service<Rom_session_component> Rom_service;

	using namespace Genode;
}

/**
 * Clone a ROM dataspace in RAM
 */
static Genode::Capability<Genode::Ram_dataspace>
clone_rom(Genode::Capability<Genode::Rom_dataspace> rom_cap)
{
	using namespace Genode;

	Genode::size_t            rom_size  = Dataspace_client(rom_cap).size();
	Capability<Ram_dataspace> clone_cap = env()->ram_session()->alloc(rom_size);

	if (!clone_cap.valid()) {
		error(__func__, ": memory allocation for cloned dataspace failed");
		return Capability<Ram_dataspace>();
	}

	void *rom   = env()->rm_session()->attach(rom_cap);
	void *clone = env()->rm_session()->attach(clone_cap);

	Genode::memcpy(clone, rom, rom_size);

	env()->rm_session()->detach(rom);
	env()->rm_session()->detach(clone);

	return clone_cap;
}


/**
 * ROM session backed by RAM dataspace copy of original ROM
 */
class Gdb_monitor::Rom_session_component : public Rpc_object<Rom_session>
{
	private:

		Genode::Rpc_entrypoint &_ep;

		Capability<Ram_dataspace> _clone_cap;

	public:

		Rom_session_component(Genode::Rpc_entrypoint &ep, char const *filename)
		: _ep(ep),
		  _clone_cap(clone_rom(Rom_connection(filename).dataspace()))
		{ _ep.manage(this); }

		~Rom_session_component()
		{
			env()->ram_session()->free(_clone_cap);
			_ep.dissolve(this);
		}

		/***************************
		 ** ROM session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override
		{
			return static_cap_cast<Rom_dataspace>(
				   static_cap_cast<Dataspace>(_clone_cap));
		}

		void sigh(Genode::Signal_context_capability) override { }
};


class Gdb_monitor::Local_rom_factory : public Rom_service::Factory
{
	private:

		Genode::Env            &_env;
		Genode::Rpc_entrypoint &_ep;

	public:

		Local_rom_factory(Genode::Env &env, Genode::Rpc_entrypoint &ep)
		: _env(env), _ep(ep) { }

		/***********************
		 ** Factory interface **
		 ***********************/

		Rom_session_component &create(Args const &args, Affinity) override
		{
			Session_label const label = label_from_args(args.string());

			return *new (env()->heap())
				Rom_session_component(_ep, label.last_element().string());
		}

		void upgrade(Rom_session_component &, Args const &) override { }

		void destroy(Rom_session_component &session) override
		{
			Genode::destroy(env()->heap(), &session);
		}
};


#endif /* _ROM_H_ */
