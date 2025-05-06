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
#include <os/buffered_xml.h>


namespace Dynamic_rom {

	using namespace Genode;

	class  Session_component;
	class  Root;
	struct Main;
}


class Dynamic_rom::Session_component : public Rpc_object<Genode::Rom_session>
{
	private:

		Env                      &_env;
		bool               const &_verbose;
		Buffered_xml       const  _rom_node;
		Timer::Connection         _timer { _env };
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

		auto _with_step(unsigned const n, auto const &fn) const
		{
			unsigned i = 0;
			_rom_node.xml.for_each_sub_node([&] (Xml_node const &step) {
				if (i++ == n)
					fn(step); });
		}

		/**
		 * Print message prefixed with ROM name
		 */
		template <typename... ARGS>
		void _log(ARGS&&... args)
		{
			if (!_verbose)
				return;

			using Name = Genode::String<160>;
			Genode::log(_rom_node.xml.attribute_value("name", Name()), ": ", args...);
		}

		enum Execution_state { EXEC_CONTINUE, EXEC_BLOCK };

		Execution_state _execute_step(Xml_node const &curr_step)
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
					using Desc = Genode::String<200>;
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

				Genode::uint64_t const milliseconds =
					curr_step.attribute_value("milliseconds", (Genode::uint64_t)0);

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

				_with_step(_curr_idx, [&] (Xml_node const &step) {
					exec_state = _execute_step(step); });

				/* advance step index, wrap at the end */
				_curr_idx = (_curr_idx + 1) % (unsigned)_rom_node.xml.num_sub_nodes();
			}
		}

		void _handle_timer() { _execute_steps_until_sleep(); }

		Entrypoint &_ep = _env.ep();

		using This = Session_component;
		Signal_handler<This> _timer_handler = { _ep, *this, &This::_handle_timer };

	public:

		Session_component(Env &env, Allocator &alloc, Xml_node const &rom_node,
		                  bool const &verbose)
		:
			_env(env), _verbose(verbose), _rom_node(alloc, rom_node)
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
			_ram_ds.construct(_env.ram(), _env.rm(), _rom_node.xml.size());

			/* fill with content of current step */
			_with_step(_last_content_idx, [&] (Xml_node const &step_node) {
				step_node.with_raw_content([&] (char const *start, size_t size) {
					memcpy(_ram_ds->local_addr<char>(), start, size); }); });

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

		Env &_env;

		bool const &_verbose;

		Buffered_xml const _config_node;

		void _with_rom_node(Genode::Session_label const &name,
		                    auto const &fn, auto const &missing_fn) const
		{
			bool found = false;
			_config_node.xml.for_each_sub_node([&] (Xml_node const &node) {
				if (!found && node.attribute_value("name", Genode::String<64>()) == name) {
					fn(node);
					found = true;
				}
			});
			if (!found)
				missing_fn();
		}

	protected:

		Create_result _create_session(const char *args) override
		{
			using namespace Genode;

			/* read name of ROM module from args */
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			Session_component *result = nullptr;

			_with_rom_node(module_name,
				[&] (Xml_node const &rom_node) {
					result = new (md_alloc())
						Session_component(_env, *md_alloc(), rom_node, _verbose); },
				[&] {
					error("ROM module lookup of '", label.string(), "' failed");
					throw Service_denied(); });

			return *result;
		}

	public:

		Root(Env &env, Genode::Allocator &md_alloc,
		     Xml_node const &config_node, bool const &verbose)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _verbose(verbose), _config_node(md_alloc, config_node)
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
