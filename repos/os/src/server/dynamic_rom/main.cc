/*
 * \brief  ROM server that changes ROM content driven by time
 * \author Norman Feske
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/reconstructible.h>
#include <util/arg_string.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/session_label.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <rom_session/rom_session.h>
#include <timer_session/connection.h>
#include <root/component.h>


namespace Dynamic_rom {
	using Genode::Entrypoint;
	using Genode::Rpc_object;
	using Genode::Sliced_heap;
	using Genode::Env;
	using Genode::Constructible;
	using Genode::Signal_context_capability;
	using Genode::Signal_handler;
	using Genode::Xml_node;
	using Genode::Arg_string;

	class  Session_component;
	class  Root;
	struct Main;
}


class Dynamic_rom::Session_component : public Rpc_object<Genode::Rom_session>
{
	private:

		Env                      &_env;
		bool                     &_verbose;
		Xml_node                  _rom_node;
		Timer::Connection         _timer;
		unsigned                  _curr_idx = 0;
		bool                      _has_content = false;
		unsigned                  _last_content_idx = ~0;
		Signal_context_capability _sigh { };

		Constructible<Genode::Attached_ram_dataspace> _ram_ds { };

		void _notify_client()
		{
			if (!_sigh.valid())
				return;

			Genode::Signal_transmitter(_sigh).submit();
		}

		/**
		 * Print message prefixed with ROM name
		 */
		template <typename... ARGS>
		void _log(ARGS&&... args)
		{
			if (!_verbose)
				return;

			typedef Genode::String<160> Name;
			Genode::log(_rom_node.attribute_value("name", Name()), ": ", args...);
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
					typedef Genode::String<200> Desc;
					Desc desc = curr_step.attribute_value("description", Desc());
					_log("change (", desc.string(), ")");
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
				_log("sleep ", milliseconds, " milliseconds");

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

		void _handle_timer() { _execute_steps_until_sleep(); }

		Entrypoint &_ep;

		typedef Session_component This;
		Signal_handler<This> _timer_handler = { _ep, *this, &This::_handle_timer };

	public:

		Session_component(Env &env, Xml_node rom_node, bool &verbose)
		:
			_env(env), _verbose(verbose), _rom_node(rom_node), _timer(env), _ep(env.ep())
		{
			/* init timer signal handler */
			_timer.sigh(_timer_handler);

			/* execute first step immediately at session-creation time */
			_execute_steps_until_sleep();
		}

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

			if (!_has_content)
				return Rom_dataspace_capability();

			/* replace dataspace by new one */
			_ram_ds.construct(_env.ram(), _env.rm(), _rom_node.size());

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

		Env        &_env;
		Xml_node    _config_node;
		bool       &_verbose;

		class Nonexistent_rom_module { };

		Xml_node _lookup_rom_node_in_config(Genode::Session_label const &name)
		{
			/* lookup ROM module in config */
			for (unsigned i = 0; i < _config_node.num_sub_nodes(); i++) {
				Xml_node node = _config_node.sub_node(i);
				if (node.has_attribute("name")
				 && node.attribute("name").has_value(name.string()))
					return node;
			}
			throw Nonexistent_rom_module();
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			/* read name of ROM module from args */
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			try {
				return new (md_alloc())
					Session_component(_env,
					                  _lookup_rom_node_in_config(module_name),
					                  _verbose);
			}
			catch (Nonexistent_rom_module) {
				error("ROM module lookup of '", label.string(), "' failed");
				throw Service_denied();
			}
		}

	public:

		Root(Env &env, Genode::Allocator &md_alloc,
		     Xml_node config_node, bool &verbose)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _config_node(config_node), _verbose(verbose)
		{ }
};


struct Dynamic_rom::Main
{
	Env &env;

	Genode::Attached_rom_dataspace config { env, "config" };

	bool verbose = config.xml().attribute_value("verbose", false);

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env, sliced_heap, config.xml(), verbose };

	Main(Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Dynamic_rom::Main main(env); }
