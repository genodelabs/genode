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
	class Rom_root;
	class Rom_service;
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

		Capability<Ram_dataspace> _clone_cap;

	public:

		Rom_session_component(char const *filename)
		: _clone_cap(clone_rom(Rom_connection(filename).dataspace()))
		{ }

		~Rom_session_component() { env()->ram_session()->free(_clone_cap); }

		Capability<Rom_dataspace> dataspace()
		{
			return static_cap_cast<Rom_dataspace>(
				   static_cap_cast<Dataspace>(_clone_cap));
		}

		void release() { }

		void sigh(Genode::Signal_context_capability) { }
};


class Gdb_monitor::Rom_root : public Root_component<Rom_session_component>
{
	protected:

		Rom_session_component *_create_session(char const *args)
		{
			Session_label const label = label_from_args(args);

			return new (md_alloc())
				Rom_session_component(label.last_element().string());
		}

	public:

		Rom_root(Rpc_entrypoint *session_ep,
				 Allocator      *md_alloc)
		: Root_component<Rom_session_component>(session_ep, md_alloc)
		{ }
};


class Gdb_monitor::Rom_service : public Service
{
	private:

		Rom_root _root;

	public:

		Rom_service(Rpc_entrypoint *entrypoint,
					Allocator      *md_alloc)
		: Service("ROM"), _root(entrypoint, md_alloc)
		{ }

		Capability<Session> session(char const *args, Affinity const &affinity) {
			return _root.session(args, affinity); }

		void upgrade(Capability<Session>, char const *) { }

		void close(Capability<Session> cap) { _root.close(cap); }
};

#endif /* _ROM_H_ */
