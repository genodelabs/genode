/*
 * \brief  ROM server that generates a ROM depending on other ROMs
 * \author Norman Feske
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <root/component.h>
#include <os/dynamic_rom_session.h>

/* local includes */
#include "input_rom_registry.h"

namespace Rom_filter {

	using namespace Genode;

	using Rom_session  = Registered<Dynamic_rom_session>;
	using Rom_sessions = Registry<Rom_session>;

	class  Root;
	struct Main;
}


struct Rom_filter::Root : Root_component<Rom_session>
{
		using Producer = Dynamic_rom_session::Producer;

		Env &_env;

		Producer &_producer;

		Rom_sessions _sessions { };

		Create_result _create_session(const char *) override
		{
			/*
			 * We ignore the name of the ROM module requested
			 */
			return *new (md_alloc()) Rom_session(_sessions, _env.ep().rpc_ep(),
			                                     _env.ram(), _env.rm(), _producer);
		}

		Root(Env &env, Allocator &md_alloc, Producer &producer)
		:
			Root_component<Rom_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _producer(producer)
		{ }

		void notify_clients()
		{
			_sessions.for_each([&] (Rom_session &session) {
				session.trigger_update(); });
		}
};


struct Rom_filter::Main : Input_rom_registry::Action
{
	Env &_env;

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Heap _heap { _env.ram(), _env.rm() };

	Input_rom_registry _input_rom_registry { _env, _heap, *this };

	void _generate_node(Node const &node, Generator &) const;

	void _generate(Generator &g) const
	{
		_config.node().with_optional_sub_node("output", [&] (Node const &output) {
			_generate_node(output, g); });
	}

	struct Producer : Dynamic_rom_session::Producer
	{
		Main &_main;

		Producer(Main &main, Generator::Type const &type)
		: Dynamic_rom_session::Producer(type), _main(main) { }

		/**
		 * Dynamic_rom_session::Producer interface
		 */
		void generate(Generator &g) override { _main._generate(g); }
	};

	Reconstructible<Producer> _producer { *this, "undefined" };

	Root _root = { _env, _sliced_heap, *_producer };

	Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();

		Node const config = _config.node();

		_verbose = config.attribute_value("verbose", false);

		Generator::Type node_type { };
		config.with_optional_sub_node("output", [&] (Node const &output) {
			node_type = output.attribute_value("node", Generator::Type()); });

		if (node_type.length() <= 1) {
			warning("missing 'node' attribute in '<output>' node");
			node_type = "undefined";
		}

		_producer.construct(*this, node_type);

		_input_rom_registry.update_config(config);

		_root.notify_clients();
	}

	/**
	 * Input_rom_registry::Action interface
	 *
	 * Called each time one of the input ROM modules changes.
	 */
	void input_rom_changed() override { _root.notify_clients(); }

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Rom_filter::Main::_generate_node(Node const &node, Generator &g) const
{
	auto process_output_sub_node = [&] (Node const &node) {

		if (node.has_type("if")) {

			/*
			 * Check condition
			 */
			bool condition_satisfied = false;

			node.with_optional_sub_node("has_value", [&] (Node const &has_value_node) {

				Input_name const input_name =
					has_value_node.attribute_value("input", Input_name());

				Input_value const expected_input_value =
					has_value_node.attribute_value("value", Input_value());

				_input_rom_registry.query_value(_config.node(), input_name).with_result(
					[&] (Input_value const &value) {
						if (value == expected_input_value)
							condition_satisfied = true; },
					[&] (Input_rom_registry::Missing) {
						if (_verbose)
							warning("could not obtain input value for input ", input_name);
					});
			});

			if (condition_satisfied) {
				node.with_optional_sub_node("then", [&] (Node const &then_node) {
					_generate_node(then_node, g); });
			} else {
				node.with_optional_sub_node("else", [&] (Node const &else_node) {
					_generate_node(else_node, g); });
			}
		}
		else if (node.has_type("attribute")) {

			using String = String<128>;

			/* assign input value to attribute value */
			if (node.has_attribute("input")) {

				Input_name const input_name =
					node.attribute_value("input", Input_name());

				_input_rom_registry.query_value(_config.node(), input_name).with_result(
					[&] (Input_value const value) {
						g.attribute(node.attribute_value("name", String()).string(),
						            value); },
					[&] (Input_rom_registry::Missing) {
						if (_verbose)
							warning("could not obtain input value for input ", input_name);
					}
				);
			} else {
				/* assign fixed attribute value */
				g.attribute(node.attribute_value("name",  String()).string(),
				            node.attribute_value("value", String()).string());
			}
		}
		else if (node.has_type("node")) {
			g.node(node.attribute_value("type", String<128>()).string(), [&]() {
				_generate_node(node, g); });
		}
		else if (node.has_type("inline")) {
			if (!g.append_node_content(node, Generator::Max_depth { 20 }))
				warning("node is too deeply nested: ", node);
		}
		else if (node.has_type("input")) {

			Input_name const input_name =
				node.attribute_value("name", Input_name());

			bool const skip_toplevel =
				node.attribute_value("skip_toplevel", false);

			_input_rom_registry.generate(input_name, g, skip_toplevel);
		}
	};

	node.for_each_sub_node(process_output_sub_node);
}


void Component::construct(Genode::Env &env) { static Rom_filter::Main main(env); }

