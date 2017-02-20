/*
 * \brief  ROM prefetching service
 * \author Norman Feske
 * \date   2011-01-24
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <root/component.h>
#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <base/session_label.h>

namespace Rom_prefetcher {
	class Rom_session_component;
	class Rom_root;
	struct Main;
}


volatile int dummy;


static void prefetch_dataspace(Genode::Region_map &rm, Genode::Dataspace_capability cap)
{
	Genode::Attached_dataspace ds(rm, cap);

	/*
	 * Modify global volatile 'dummy' variable to prevent the compiler
	 * from removing the prefetch loop.
	 */

	enum { PREFETCH_STEP = 4096 };
	for (Genode::size_t i = 0; i < ds.size(); i += PREFETCH_STEP)
		dummy += ds.local_addr<char>()[i];
}


class Rom_prefetcher::Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		Genode::Rom_connection _rom;

	public:

		/**
		 * Constructor
		 *
		 * \param  filename  name of the requested file
		 */
		Rom_session_component(Genode::Env &env, Genode::Session_label const &label)
		:
			_rom(env, label.string())
		{
			prefetch_dataspace(env.rm(), _rom.dataspace());
		}


		/***************************
		 ** ROM session interface **
		 ***************************/

		Genode::Rom_dataspace_capability dataspace() { return _rom.dataspace(); }

		void sigh(Genode::Signal_context_capability) { }
};


class Rom_prefetcher::Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		Genode::Env &_env;

		Rom_session_component *_create_session(const char *args)
		{
			Genode::Session_label const label = Genode::label_from_args(args);

			/* create new session for the requested file */
			return new (md_alloc())
				Rom_session_component(_env, label.last_element());
		}

	public:

		Rom_root(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Rom_session_component>(env.ep(), md_alloc),
			_env(env)
		{ }
};


struct Rom_prefetcher::Main
{
	Genode::Env &_env;

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Rom_root _root { _env, _sliced_heap };

	Main(Genode::Env &env) : _env(env)
	{
		Timer::Connection timer(_env);

		_config.xml().for_each_sub_node("rom", [&] (Genode::Xml_node entry) {

			typedef Genode::String<64> Name;
			Name const name = entry.attribute_value("name", Name());

			try {
				Genode::Rom_connection rom(_env, name.string());
				log("prefetching ROM module ", name);
				prefetch_dataspace(_env.rm(), rom.dataspace());
			} catch (...) {
				error("could not open ROM module ", name);
			}

			/* yield */
			timer.msleep(1);
		});

		/* announce server */
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Rom_prefetcher::Main main(env); }

