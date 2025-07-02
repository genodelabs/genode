/*
 * \brief  Registry of ROM modules used as input for the condition
 * \author Norman Feske
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_ROM_REGISTRY_H_
#define _INPUT_ROM_REGISTRY_H_

/* Genode includes */
#include <util/xml_node.h>
#include <base/attached_rom_dataspace.h>
#include <base/allocator.h>

namespace Rom_filter {

	class Input_rom_registry;

	using Input_rom_name = Genode::String<100>;
	using Input_name     = Genode::String<100>;
	using Input_value    = Genode::String<100>;
	using Node_type_name = Genode::String<80>;
	using Attribute_name = Genode::String<80>;

	using Genode::Signal_context_capability;
	using Genode::Signal_handler;
	using Genode::Xml_node;
	using Genode::Interface;
}


class Rom_filter::Input_rom_registry
{
	public:

		/**
		 * Callback type
		 */
		struct Input_rom_changed_fn : Interface
		{
			virtual void input_rom_changed() = 0;
		};

		struct Missing { };
		using Query_result = Genode::Attempt<Input_value, Missing>;


	private:

		class Entry : public Genode::List<Entry>::Element
		{
			private:

				Genode::Env &_env;

				Input_rom_name _name;

				Input_rom_changed_fn &_input_rom_changed_fn;

				Genode::Attached_rom_dataspace _rom_ds { _env, _name.string() };

				void _handle_rom_changed()
				{
					_rom_ds.update();
					if (!_rom_ds.valid())
						return;

					/* trigger re-evaluation of the inputs */
					_input_rom_changed_fn.input_rom_changed();
				}

				Genode::Signal_handler<Entry> _rom_changed_handler =
					{ _env.ep(), *this, &Entry::_handle_rom_changed };

				static void _with_any_sub_node(Xml_node const &node, auto const &fn)
				{
					bool first = true;
					node.for_each_sub_node([&] (Xml_node const &sub_node) {
						if (first)
							fn(sub_node);
						first = false;
					});
				}

				static void _with_matching_sub_node(Node_type_name type,
				                                    Xml_node const &path,
				                                    Xml_node const &content,
				                                    auto const &fn,
				                                    auto const &missing_fn)
				{
					using Attribute_value = Input_value;

					Attribute_name const expected_attr =
						path.attribute_value("attribute", Attribute_name());

					Attribute_value const expected_value =
						path.attribute_value("value", Attribute_value());

					bool found = false;
					content.for_each_sub_node(type.string(), [&] (Xml_node const &sub_node) {
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
							_with_any_sub_node(path, [&] (Xml_node const &sub_path) {
								found = true;
								fn(sub_node, sub_path);
							});
					});
					if (!found)
						missing_fn();
				}

				/**
				 * Query value from XML-structured ROM content
				 *
				 * \param path     XML node that defines the path to the value
				 * \param content  XML-structured content, to which the path
				 *                 is applied
				 */
				Query_result _query_value(Xml_node const &path, Xml_node const &content,
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
							[&] (Xml_node const &sub_content, Xml_node const &sub_path) {
								result = _query_value(sub_path, sub_content, max_depth - 1);
							},
							[&] { }
						);
						return result;
					}
					return Missing();
				}

				/**
				 * Return the expected top-level XML node type of a given input
				 */
				static Node_type_name _top_level_node_type(Xml_node const &input_node)
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
				Entry(Genode::Env &env, Input_rom_name const &name,
				      Input_rom_changed_fn &input_rom_changed_fn)
				:
					_env(env), _name(name),
					_input_rom_changed_fn(input_rom_changed_fn)
				{
					_rom_ds.sigh(_rom_changed_handler);
				}

				Input_rom_name name() const { return _name; }

				/**
				 * Query input value from ROM modules
				 *
				 * \param input_node  XML that describes the path to the
				 *                    input value
				 */
				Query_result query_value(Xml_node const &input_node) const
				{
					Xml_node const &content_node = _rom_ds.xml();

					/*
					 * Check type of top-level node, query value of the
					 * type name matches.
					 */
					Node_type_name expected = _top_level_node_type(input_node);
					if (content_node.has_type(expected.string())) {
						Query_result result = Missing();
						_with_any_sub_node(input_node, [&] (Xml_node const &sub_node) {
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
					Xml_node const &node = _rom_ds.xml();
					if (node.type() == "empty")
						missing_fn();
					else
						fn(node);
				}
		};

		Genode::Allocator &_alloc;

		Genode::Env &_env;

		Genode::List<Entry> _input_roms { };

		Input_rom_changed_fn &_input_rom_changed_fn;

		/**
		 * Apply functor for each input ROM
		 *
		 * The functor is called with 'Input &' as argument.
		 */
		template <typename FUNC>
		void _for_each_input_rom(FUNC const &func) const
		{
			Entry const *ir   = _input_roms.first();
			Entry const *next = nullptr;
			for (; ir; ir = next) {

				/*
				 * Obtain next element prior calling the functor because
				 * the functor may remove the current element from the list.
				 */
				next = ir->next();

				func(*ir);
			}
		}

		/**
		 * Return ROM name of specified XML node
		 */
		static inline Input_rom_name _input_rom_name(Xml_node const &input)
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
		 * Return true if ROM with specified name is known
		 */
		bool _input_rom_exists(Input_rom_name const &name) const
		{
			bool result = false;

			_for_each_input_rom([&] (Entry const &input_rom) {

				if (input_rom.name() == name)
					result = true;
			});

			return result;
		}

		static bool _config_uses_input_rom(Xml_node const &config,
		                                   Input_rom_name const &name)
		{
			bool result = false;

			config.for_each_sub_node("input", [&] (Xml_node const &input) {

				if (_input_rom_name(input) == name)
					result = true;
			});

			return result;
		}

		void _with_entry_by_name(Input_rom_name const &name, auto const &fn) const
		{
			bool first = true;
			_for_each_input_rom([&] (Entry const &input_rom) {
				if (first && input_rom.name() == name) {
					first = false;
					fn(input_rom);
				}
			});
		}

		Query_result _query_value_in_roms(Xml_node const &input_node) const
		{
			Query_result result = Missing();
			_with_entry_by_name(_input_rom_name(input_node), [&] (Entry const &entry) {
				result = entry.query_value(input_node); });
			return result;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param sigh  signal context capability to install in ROM sessions
		 *              for the inputs
		 */
		Input_rom_registry(Genode::Env &env, Genode::Allocator &alloc,
		                   Input_rom_changed_fn &input_rom_changed_fn)
		:
			_alloc(alloc), _env(env), _input_rom_changed_fn(input_rom_changed_fn)
		{ }

		void update_config(Xml_node const &config)
		{
			/*
			 * Remove ROMs that are no longer present in the configuration.
			 */
			auto remove_stale_entry = [&] (Entry const &entry) {

				if (_config_uses_input_rom(config, entry.name()))
					return;

				_input_roms.remove(const_cast<Entry *>(&entry));
				Genode::destroy(_alloc, const_cast<Entry *>(&entry));
			};
			_for_each_input_rom(remove_stale_entry);

			/*
			 * Add new appearing ROMs.
			 */
			auto add_new_entry = [&] (Xml_node const &input) {

				Input_rom_name name = _input_rom_name(input);

				if (_input_rom_exists(name))
					return;

				Entry *entry =
					new (_alloc) Entry(_env, name, _input_rom_changed_fn);

				_input_roms.insert(entry);
			};
			config.for_each_sub_node("input", add_new_entry);
		}

		/**
		 * Lookup value of input with specified name
		 */
		Query_result query_value(Xml_node const &config, Input_name const &input_name) const
		{
			Query_result result = Missing();
			config.for_each_sub_node("input", [&] (Xml_node const &input_node) {
				if (input_node.attribute_value("name", Input_name("")) == input_name)
					result = _query_value_in_roms(input_node); });
			return result;
		}

		/**
		 * Generate content of the specifed input
		 */
		void gen_xml(Input_name const &input_name, Genode::Xml_generator &xml, bool skip_toplevel=false)
		{
			_with_entry_by_name(input_name, [&] (Entry const &entry) {
				entry.with_node(
					[&] (Xml_node const &node) {
						Genode::Xml_generator::Max_depth const max_depth { 20 };
						bool const ok = skip_toplevel
						              ? xml.append_node_content(node, max_depth)
						              : xml.append_node        (node, max_depth);
						if (!ok)
							warning("node too deeply nested: ", node);
					},
					[&] { }
				);
			});
		}
};

#endif /* _INPUT_ROM_REGISTRY_H_ */
