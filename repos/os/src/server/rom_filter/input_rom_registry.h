/*
 * \brief  Registry of ROM modules used as input for the condition
 * \author Norman Feske
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015-2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_ROM_REGISTRY_H_
#define _INPUT_ROM_REGISTRY_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <util/list_model.h>
#include <base/allocator.h>

namespace Rom_filter {

	using namespace Genode;

	class Input_rom_registry;

	using Input_rom_name = String<100>;
	using Input_name     = String<100>;
	using Input_value    = String<100>;
	using Node_type_name = String<80>;
	using Attribute_name = String<80>;
}


class Rom_filter::Input_rom_registry
{
	public:

		struct Action : Interface { virtual void input_rom_changed() = 0; };

		struct Missing { };
		using Query_result = Attempt<Input_value, Missing>;

	private:

		class Input : public List_model<Input>::Element
		{
			public:

				Input_rom_name name;

			private:

				Env &_env;

				Action &_action;

				Attached_rom_dataspace _rom_ds { _env, name.string() };

				void _handle_rom_changed()
				{
					_rom_ds.update();
					if (!_rom_ds.valid())
						return;

					/* trigger re-evaluation of the inputs */
					_action.input_rom_changed();
				}

				Signal_handler<Input> _rom_changed_handler =
					{ _env.ep(), *this, &Input::_handle_rom_changed };

				static void _with_any_sub_node(Node const &node, auto const &fn)
				{
					bool first = true;
					node.for_each_sub_node([&] (Node const &sub_node) {
						if (first)
							fn(sub_node);
						first = false;
					});
				}

				static void _with_matching_sub_node(Node_type_name type,
				                                    Node const &path,
				                                    Node const &content,
				                                    auto const &fn,
				                                    auto const &missing_fn)
				{
					using Attribute_value = Input_value;

					Attribute_name const expected_attr =
						path.attribute_value("attribute", Attribute_name());

					Attribute_value const expected_value =
						path.attribute_value("value", Attribute_value());

					bool found = false;
					content.for_each_sub_node(type.string(), [&] (Node const &sub_node) {
						if (found)
							return;

						auto match = [&]
						{
							/* attribute remains unspecified -> match */
							if (!expected_attr.valid())
								return true;

							/* value remains unspecified -> match */
							if (!expected_value.valid())
								return true;

							Attribute_value const present_value =
								sub_node.attribute_value(expected_attr.string(),
								                         Attribute_value());

							if (present_value == expected_value)
								return true;

							return false;
						};

						if (match())
							_with_any_sub_node(path, [&] (Node const &sub_path) {
								found = true;
								fn(sub_node, sub_path);
							});
					});
					if (!found)
						missing_fn();
				}

				/**
				 * Query value from structured ROM content
				 *
				 * \param path     node that defines the path to the value
				 * \param content  structured content, to which the path
				 *                 is applied
				 */
				Query_result _query_value(Node const &path, Node const &content,
				                          unsigned const max_depth = 10) const
				{
					if (max_depth == 0)
						return Missing();

					/*
					 * Take value of an attribute
					 */
					if (path.has_type("attribute")) {

						Attribute_name const attr_name =
							path.attribute_value("name", Attribute_name(""));

						if (!content.has_attribute(attr_name.string()))
							return Missing();

						return content.attribute_value(attr_name.string(),
						                               Input_value(""));
					}

					/*
					 * Follow path node
					 */
					if (path.has_type("node")) {

						Node_type_name const sub_node_type =
							path.attribute_value("type", Node_type_name(""));

						Query_result result = Missing();
						_with_matching_sub_node(sub_node_type, path, content,
							[&] (Node const &sub_content, Node const &sub_path) {
								result = _query_value(sub_path, sub_content, max_depth - 1);
							},
							[&] { }
						);
						return result;
					}
					return Missing();
				}

				/**
				 * Return the expected top-level node type of a given input
				 */
				static Node_type_name _top_level_node_type(Node const &input_node)
				{
					Node_type_name const undefined("");

					if (input_node.has_attribute("node"))
						return input_node.attribute_value("node", undefined);

					return input_node.attribute_value("name", undefined);
				}

			public:

				/**
				 * Constructor
				 */
				Input(Env &env, Input_rom_name const &name, Action &action)
				:
					name(name), _env(env), _action(action)
				{
					_rom_ds.sigh(_rom_changed_handler);
				}

				/**
				 * Query input value from ROM modules
				 *
				 * \param input_node  that describes the path to the
				 *                    input value
				 */
				Query_result query_value(Node const &input_node) const
				{
					Node const &content_node = _rom_ds.node();

					/*
					 * Check type of top-level node, query value of the
					 * type name matches.
					 */
					Node_type_name expected = _top_level_node_type(input_node);
					if (content_node.has_type(expected.string())) {
						Query_result result = Missing();
						_with_any_sub_node(input_node, [&] (Node const &sub_node) {
							result = _query_value(sub_node, content_node); });
						if (result.ok())
							return result;
					}

					if (input_node.has_attribute("default"))
						return input_node.attribute_value("default", Input_value(""));

					return Missing();
				}

				void with_node(auto const &fn, auto const &missing_fn) const
				{
					Node const &node = _rom_ds.node();
					if (node.type() == "empty")
						missing_fn();
					else
						fn(node);
				}

				static inline Input_rom_name name_from_node(Node const &input)
				{
					if (input.has_attribute("rom"))
						return input.attribute_value("rom", Input_rom_name(""));

					/*
					 * If no 'rom' attribute was specified, we fall back to use the
					 * name of the input as ROM name.
					 */
					return input.attribute_value("name", Input_rom_name(""));
				}

				/**
				 * List_model::Element
				 */
				static bool type_matches(Node const &node)
				{
					return node.type() == "input";
				}

				bool matches(Node const &node) const
				{
					return name_from_node(node) == name;
				}
		};

		Env       &_env;
		Allocator &_alloc;
		Action    &_action;

		List_model<Input> _inputs { };

		void _with_input_by_name(Input_rom_name const &name, auto const &fn) const
		{
			bool first = true;
			_inputs.for_each([&] (Input const &input) {
				if (first && input.name == name) {
					first = false;
					fn(input);
				}
			});
		}

		Query_result _query_value_in_roms(Node const &input_node) const
		{
			Query_result result = Missing();
			_with_input_by_name(Input::name_from_node(input_node),
				[&] (Input const &input) { result = input.query_value(input_node); });
			return result;
		}

	public:

		Input_rom_registry(Env &env, Allocator &alloc, Action &action)
		:
			_env(env), _alloc(alloc), _action(action)
		{ }

		void update_config(Node const &config)
		{
			_inputs.update_from_node(config,

				[&] (Node const &node) -> Input & {
					return *new (_alloc)
					Input(_env, Input::name_from_node(node), _action); },

				[&] (Input &input) { destroy(_alloc, &input); },

				[&] (Input &, Node const &) { /* nothing to update */ }
			);
		}

		/**
		 * Lookup value of input with specified name
		 */
		Query_result query_value(Node const &config, Input_name const &input_name) const
		{
			Query_result result = Missing();
			config.for_each_sub_node("input", [&] (Node const &input_node) {
				if (input_node.attribute_value("name", Input_name("")) == input_name)
					result = _query_value_in_roms(input_node); });
			return result;
		}

		/**
		 * Generate content of the specifed input
		 */
		void generate(Input_name const &input_name, Generator &g,
		              bool skip_toplevel = false) const
		{
			_with_input_by_name(input_name, [&] (Input const &input) {
				input.with_node(
					[&] (Node const &node) {
						Generator::Max_depth const max_depth { 20 };
						bool const ok = skip_toplevel
						              ? g.append_node_content(node, max_depth)
						              : g.append_node        (node, max_depth);
						if (!ok)
							warning("node too deeply nested: ", node);
					},
					[&] { }
				);
			});
		}
};

#endif /* _INPUT_ROM_REGISTRY_H_ */
