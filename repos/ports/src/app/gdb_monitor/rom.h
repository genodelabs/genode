/*
 * \brief  ROM service
 * \author Christian Helmuth
 * \date   2011-09-16
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
	using namespace Genode;

	class Rom_session_component;
	class Local_rom_factory;
	typedef Local_service<Rom_session_component> Rom_service;
}


/**
 * ROM session backed by RAM dataspace copy of original ROM
 */
class Gdb_monitor::Rom_session_component : public Rpc_object<Rom_session>
{
	private:

		Env                       &_env;
		Rpc_entrypoint            &_ep;

		Capability<Ram_dataspace>  _clone_cap;

		/**
 	 	 * Clone a ROM dataspace in RAM
 	 	 */
		Capability<Ram_dataspace> _clone_rom(Capability<Rom_dataspace> rom_cap)
		{
			using namespace Genode;

			size_t                    rom_size  = Dataspace_client(rom_cap).size();
			Capability<Ram_dataspace> clone_cap = _env.ram().alloc(rom_size);

			if (!clone_cap.valid()) {
				error(__func__, ": memory allocation for cloned dataspace failed");
				return Capability<Ram_dataspace>();
			}

			void *rom   = _env.rm().attach(rom_cap);
			void *clone = _env.rm().attach(clone_cap);

			memcpy(clone, rom, rom_size);

			_env.rm().detach(rom);
			_env.rm().detach(clone);

			return clone_cap;
		}


	public:

		Rom_session_component(Env            &env,
		                      Rpc_entrypoint &ep,
		                      char const     *filename)
		: _env(env),
		  _ep(ep),
		  _clone_cap(_clone_rom(Rom_connection(env, filename).dataspace()))
		{ _ep.manage(this); }

		~Rom_session_component()
		{
			_env.ram().free(_clone_cap);
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

		void sigh(Signal_context_capability) override { }
};


class Gdb_monitor::Local_rom_factory : public Rom_service::Factory
{
	private:

		Env            &_env;
		Rpc_entrypoint &_ep;
		Allocator      &_alloc;

	public:

		Local_rom_factory(Env &env, Rpc_entrypoint &ep, Allocator &alloc)
		: _env(env), _ep(ep), _alloc(alloc) { }

		/***********************
		 ** Factory interface **
		 ***********************/

		Rom_session_component &create(Args const &args, Affinity) override
		{
			Session_label const label = label_from_args(args.string());

			return *new (_alloc)
				Rom_session_component(_env,
				                      _ep,
				                      label.last_element().string());
		}

		void upgrade(Rom_session_component &, Args const &) override { }

		void destroy(Rom_session_component &session) override
		{
			Genode::destroy(_alloc, &session);
		}
};


#endif /* _ROM_H_ */
