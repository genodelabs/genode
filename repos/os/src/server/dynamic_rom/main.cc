/*
 * \brief  ROM server that changes ROM content driven by time
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/volatile_object.h>
#include <util/arg_string.h>
#include <base/heap.h>
#include <base/env.h>
#include <root/component.h>
#include <os/server.h>
#include <os/config.h>
#include <os/attached_ram_dataspace.h>
#include <os/server.h>
#include <rom_session/rom_session.h>
#include <timer_session/connection.h>


namespace Dynamic_rom {
	using Server::Entrypoint;
	using Genode::Rpc_object;
	using Genode::Sliced_heap;
	using Genode::env;
	using Genode::Lazy_volatile_object;
	using Genode::Signal_context_capability;
	using Genode::Xml_node;
	using Genode::Signal_rpc_member;
	using Genode::Arg_string;

	class  Session_component;
	class  Root;
	struct Main;
}


class Dynamic_rom::Session_component : public Rpc_object<Genode::Rom_session>
{
	private:

		bool                     &_verbose;
		Xml_node                  _rom_node;
		Timer::Connection         _timer;
		unsigned                  _curr_idx = 0;
		bool                      _has_content = false;
		unsigned                  _last_content_idx = ~0;
		Signal_context_capability _sigh;

		Lazy_volatile_object<Genode::Attached_ram_dataspace> _ram_ds;

		void _notify_client()
		{
			if (!_sigh.valid())
				return;

			Genode::Signal_transmitter(_sigh).submit();
		}

		template <typename... ARGS>
		void _log(char const *format, ARGS... args)
		{
			if (!_verbose)
				return;

			char name[200];
			_rom_node.attribute("name").value(name, sizeof(name));

			Genode::printf("%s: ", name);
			Genode::printf(format, args...);
			Genode::printf("\n");
		}

		enum Execution_state { EXEC_CONTINUE, EXEC_BLOCK };

		Execution_state _execute_step(Xml_node curr_step)
		{
			/*
			 * Replace content of ROM module by new one. Note that the content
			 * of the currently handed out dataspace remains untouched until
			 * the ROM client requests the new version by calling 'dataspace'
			 * the next time.
			 */
			if (curr_step.has_type("inline")) {
				_last_content_idx = _curr_idx;
				_has_content      = true;
				_notify_client();

				if (curr_step.has_attribute("description")) {
					char desc[200];
					desc[0] = 0;
					curr_step.attribute("description").value(desc, sizeof(desc));
					_log("change (%s)", desc);
				} else {
					_log("change");
				}
			}

			/*
			 * Remove ROM module
			 */
			if (curr_step.has_type("empty")) {
				_last_content_idx = ~0;;
				_has_content      = false;
				_notify_client();
				_log("remove");
			}

			/*
			 * Sleep some time
			 *
			 * The timer will trigger the execution of the next step.
			 */
			if (curr_step.has_type("sleep")
			 && curr_step.has_attribute("milliseconds")) {

				unsigned long milliseconds = 0;
				curr_step.attribute("milliseconds").value(&milliseconds);
				_timer.trigger_once(milliseconds*1000);
				_log("sleep %ld milliseconds", milliseconds);

				return EXEC_BLOCK;
			}

			return EXEC_CONTINUE;
		}

		void _execute_steps_until_sleep()
		{
			Execution_state exec_state = EXEC_CONTINUE;
			while (exec_state == EXEC_CONTINUE) {

				exec_state = _execute_step(_rom_node.sub_node(_curr_idx));

				/* advance step index, wrap at the end */
				_curr_idx = (_curr_idx + 1) % _rom_node.num_sub_nodes();
			}
		}

		void _timer_handler(unsigned) { _execute_steps_until_sleep(); }

		Entrypoint &_ep;

		typedef Session_component This;
		Signal_rpc_member<This> _timer_dispatcher = { _ep, *this, &This::_timer_handler };

	public:

		Session_component(Entrypoint &ep, Xml_node rom_node, bool &verbose)
		:
			_verbose(verbose), _rom_node(rom_node), _ep(ep)
		{
			/* init timer signal handler */
			_timer.sigh(_timer_dispatcher);

			/* execute first step immediately at session-creation time */
			_execute_steps_until_sleep();
		}

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

			if (!_has_content)
				return Rom_dataspace_capability();

			/* replace dataspace by new one */
			_ram_ds.construct(env()->ram_session(), _rom_node.size());

			/* fill with content of current step */
			Xml_node step_node = _rom_node.sub_node(_last_content_idx);
			memcpy(_ram_ds->local_addr<char>(),
			       step_node.content_addr(),
			       step_node.content_size());

			/* cast RAM into ROM dataspace capability */
			Dataspace_capability ds_cap = static_cap_cast<Dataspace>(_ram_ds->cap());
			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		void sigh(Genode::Signal_context_capability sigh) override
		{
			_sigh = sigh;
		}
};


class Dynamic_rom::Root : public Genode::Root_component<Session_component>
{
	private:

		Entrypoint &_ep;
		Xml_node    _config_node;
		bool       &_verbose;

		class Nonexistent_rom_module { };

		Xml_node _lookup_rom_node_in_config(char const *name)
		{
			/* lookup ROM module in config */
			for (unsigned i = 0; i < _config_node.num_sub_nodes(); i++) {
				Xml_node node = _config_node.sub_node(i);
				if (node.has_attribute("name")
				 && node.attribute("name").has_value(name))
					return node;
			}
			throw Nonexistent_rom_module();
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			/* read name of ROM module from args */
			char name[200];
			Arg_string::find_arg(args, "filename").string(name, sizeof(name), "");

			try {
				return new (md_alloc())
					Session_component(_ep,
					                  _lookup_rom_node_in_config(name),
					                  _verbose);

			} catch (Nonexistent_rom_module) {
				PERR("ROM module lookup for \"%s\" failed.", name);
				throw Root::Invalid_args();
			}
		}

	public:

		Root(Entrypoint &ep, Genode::Allocator &md_alloc,
		     Xml_node config_node, bool &verbose)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep), _config_node(config_node), _verbose(verbose)
		{ }
};


struct Dynamic_rom::Main
{
	bool _verbose_config()
	{
		Xml_node config = Genode::config()->xml_node();

		return config.has_attribute("verbose")
		    && config.attribute("verbose").has_value("yes");
	}

	bool verbose = _verbose_config();

	Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root root = { ep, sliced_heap, Genode::config()->xml_node(), verbose };

	Main(Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(root));
	}
};


namespace Server {

	char const *name() { return "dynamic_rom_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Dynamic_rom::Main main(ep);
	}
}
