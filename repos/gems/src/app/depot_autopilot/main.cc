/*
 * \brief  Tool for turning a subsystem blueprint into an init configuration
 * \author Norman Feske
 * \date   2017-07-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <root/component.h>

/* local includes */
#include "children.h"

namespace Depot_deploy {

	class  Log_session_component;
	class  Log_root;
	struct Main;
}


class Depot_deploy::Log_session_component : public Rpc_object<Log_session>
{
	private:

		Session_label const  _label;
		Child               &_child;

	public:

		Log_session_component(Session_label const &label,
		                      Child               &child)
		:
			_label(label),
			_child(child)
		{ }

		void write(String const &line) override
		{
			_child.log_session_write(line, _label);
		}
};


class Depot_deploy::Log_root : public Root_component<Log_session_component>
{
	public:

		Children            &_children;
		Session_label const &_children_label_prefix;

		Log_root(Entrypoint          &ep,
		         Allocator           &md_alloc,
		         Children            &children,
		         Session_label const &children_label_prefix)
		:
			Root_component         { ep, md_alloc },
			_children              { children },
			_children_label_prefix { children_label_prefix }
		{ }

		Log_session_component *_create_session(const char *args, Affinity const &) override
		{
			using Name_delimiter = String<5>;

			Session_label const label            { label_from_args(args) };
			size_t        const label_prefix_len { strlen(_children_label_prefix.string()) };
			Session_label const label_prefix     { Cstring(label.string(), label_prefix_len) };

			if (label_prefix != _children_label_prefix) {
				warning("LOG session label does not have children label-prefix");
				throw Service_denied();
			}
			char    const *const name_base  { label.string() + label_prefix_len };
			Name_delimiter const name_delim { " -> " };

			size_t name_len { 0 };
			for (char const *str = name_base; str[name_len] &&
			     name_delim != Name_delimiter(str + name_len); name_len++);

			Child::Name       name       { Cstring(name_base, name_len) };
			char const *const label_base { name_base + name_len };

			try {
				return new (md_alloc())
					Log_session_component(Session_label("init", label_base),
					                      _children.find_by_name(name));
			}
			catch (Children::No_match) {
				warning("Cannot find child by LOG session label");
				throw Service_denied();
			}
		}
};


struct Depot_deploy::Main
{
	struct Repeatable
	{
		Env                            &_env;
		Heap                           &_heap;
		Signal_transmitter              _repeat_handler;
		Attached_rom_dataspace          _config                { _env, "config" };
		Attached_rom_dataspace          _blueprint             { _env, "blueprint" };
		Expanding_reporter              _query_reporter        { _env, "query" , "query"};
		Expanding_reporter              _init_config_reporter  { _env, "config", "init.config"};
		Reconstructible<Session_label>  _children_label_prefix { };
		Timer::Connection               _timer                 { _env };
		Signal_handler<Repeatable>      _config_handler        { _env.ep(), *this, &Repeatable::_handle_config };
		Children                        _children              { _heap, _timer, _config_handler };

		void _handle_config()
		{
			_config.update();
			_blueprint.update();

			Xml_node const config = _config.xml();

			_children_label_prefix.construct(config.attribute_value("children_label_prefix", String<160>()));
			_children.apply_config(config);
			_children.apply_blueprint(_blueprint.xml());

			/* determine CPU architecture of deployment */
			typedef String<16> Arch;
			Arch const arch = config.attribute_value("arch", Arch());
			if (!arch.valid())
				warning("config lacks 'arch' attribute");

			/* generate init config containing all configured start nodes */
			bool finished;
			_init_config_reporter.generate([&] (Xml_generator &xml) {
				Xml_node static_config = config.sub_node("static");
				static_config.with_raw_content([&] (char const *start, size_t length) {
					xml.append(start, length); });
				Child::Depot_rom_server const parent { };
				finished = _children.gen_start_nodes(xml, config.sub_node("common_routes"),
				                                     parent, parent);
			});
			if (finished) {

				Result result;
				Genode::uint64_t previous_time_sec { 0 };
				if (config.has_sub_node("previous-results")) {
					Xml_node const previous_results = config.sub_node("previous-results");
					previous_time_sec += previous_results.attribute_value("time_sec", (Genode::uint64_t)0);
					result.succeeded  += previous_results.attribute_value("succeeded", 0UL);
					result.failed     += previous_results.attribute_value("failed", 0UL);
					result.skipped    += previous_results.attribute_value("skipped", 0UL);
				}
				Genode::uint64_t const time_us  { _timer.curr_time().trunc_to_plain_us().value };
				Genode::uint64_t       time_ms  { time_us / 1000 };
				Genode::uint64_t const time_sec { time_ms / 1000 };
				time_ms = time_ms - time_sec * 1000;

				_children.conclusion(result);
				int exit_code = result.failed ? -1 : 0;

				typedef String<12> Repeat;
				Repeat repeat = config.attribute_value("repeat", Repeat("false"));
				if (repeat == Repeat("until_forever") ||
				    (repeat == Repeat("until_failed") && exit_code == 0)) {

					char const *empty_config_str = "<config/>";
					Xml_node const empty_config(empty_config_str, 10);
					_children.apply_config(empty_config);
					_query_reporter.generate([&] (Xml_generator &xml) { xml.attribute("arch", arch); });
					_init_config_reporter.generate([&] (Xml_generator &) { });
					_repeat_handler.submit();
					return;

				} else {

					log("\n--- Finished after ", time_sec + previous_time_sec, ".", time_ms < 10 ? "00" : time_ms < 100 ? "0" : "", time_ms, " sec ---\n");
					if (config.has_sub_node("previous-results")) {
						Xml_node const previous_results = config.sub_node("previous-results");
						previous_results.with_raw_content([&] (char const *start, size_t length) {
							if (length)
								log(Cstring(start, length)); });
					}
					_children.print_conclusion();
					log("");
					log(result);
					log("");
					_env.parent().exit(exit_code);
				}
			}

			/* update query for blueprints of all unconfigured start nodes */
			if (arch.valid()) {
				_query_reporter.generate([&] (Xml_generator &xml) {
					xml.attribute("arch", arch);
					_children.gen_queries(xml);
				});
			}
		}

		Repeatable(Env                             &env,
		           Signal_context_capability const &repeat_handler,
		           Heap                            &heap)
		:
			_env(env),
			_heap(heap),
			_repeat_handler(repeat_handler)
		{
			_config   .sigh(_config_handler);
			_blueprint.sigh(_config_handler);

			_handle_config();
		}
	};


	Env                         &_env;
	Signal_handler<Main>         _repeat_handler { _env.ep(), *this, &Main::_handle_repeat };
	Heap                         _heap           { _env.ram(), _env.rm() };
	Reconstructible<Repeatable>  _repeatable     { _env, _repeat_handler, _heap };
	Log_root                     _log_root       { _env.ep(), _heap, _repeatable->_children, *_repeatable->_children_label_prefix };

	void _handle_repeat()
	{
		_repeatable.construct(_env, _repeat_handler, _heap);
	}

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_log_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Depot_deploy::Main main(env);
}

