/*
 * \brief  ROM prefetching service
 * \author Norman Feske
 * \date   2011-01-24
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rom_session/connection.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <dataspace/client.h>
#include <base/rpc_server.h>
#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>

volatile int dummy;


class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Genode::Rom_connection _rom;

		enum { _PREFETCH_STEP = 4096 };

		void _prefetch_dataspace(Genode::Dataspace_capability ds)
		{
			char *mapped = Genode::env()->rm_session()->attach(ds);
			Genode::size_t size = Genode::Dataspace_client(ds).size();

			/*
			 * Modify global volatile 'dummy' variable to prevent the compiler
			 * from removing the prefetch loop.
			 */

			for (Genode::size_t i = 0; i < size; i += _PREFETCH_STEP)
				dummy += mapped[i];

			Genode::env()->rm_session()->detach(mapped);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  filename  name of the requested file
		 */
		Rom_session_component(const char *filename) : _rom(filename)
		{
			_prefetch_dataspace(_rom.dataspace());
		}

		/***************************
		 ** ROM session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace() {
			return _rom.dataspace(); }
};

class Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		Rom_session_component *_create_session(const char *args)
		{
			enum { FILENAME_MAX_LEN = 128 };
			char filename[FILENAME_MAX_LEN];
			Genode::Arg_string::find_arg(args, "filename").string(filename, sizeof(filename), "");

			/* create new session for the requested file */
			return new (md_alloc()) Rom_session_component(filename);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  entrypoint  entrypoint to be used for ROM sessions
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 */
		Rom_root(Genode::Rpc_entrypoint *entrypoint,
		         Genode::Allocator      *md_alloc)
		:
			Genode::Root_component<Rom_session_component>(entrypoint, md_alloc)
		{ }
};


int main(int argc, char **argv)
{
	using namespace Genode;

	/* connection to capability service needed to create capabilities */
	static Cap_connection cap;

	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());

	/* creation of the entrypoint and the root interface */
	enum { STACK_SIZE = 8*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rom_pf_ep");
	static Rom_root rom_root(&ep, &sliced_heap);

	/* announce server */
	env()->parent()->announce(ep.manage(&rom_root));

	/* wait for activation through client */
	sleep_forever();
	return 0;
};
