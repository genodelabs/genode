/*
 * \brief  ROM server that generates a ROM depending on other ROMs
 * \author Norman Feske
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/reconstructible.h>
#include <util/arg_string.h>
#include <util/xml_generator.h>
#include <util/retry.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/attached_ram_dataspace.h>
#include <root/component.h>

/* local includes */
#include "input_rom_registry.h"

namespace Rom_filter {
	using Genode::Entrypoint;
	using Genode::Rpc_object;
	using Genode::Sliced_heap;
	using Genode::Constructible;
	using Genode::Xml_generator;
	using Genode::size_t;
	using Genode::Interface;

	class  Output_buffer;
	class  Session_component;
	class  Root;
	struct Main;

	typedef Genode::List<Session_component> Session_list;
}


/**
 * Interface used by the sessions to obtain the XML output data
 */
struct Rom_filter::Output_buffer : Interface
{
	virtual size_t content_size() const = 0;
	virtual size_t export_content(char *dst, size_t dst_len) const = 0;
};


class Rom_filter::Session_component : public Rpc_object<Genode::Rom_session>,
                                      private Session_list::Element
{
	private:

		friend class Genode::List<Session_component>;

		Genode::Env &_env;

		Signal_context_capability _sigh { };

		Output_buffer const &_output_buffer;

		Session_list &_sessions;

		Constructible<Genode::Attached_ram_dataspace> _ram_ds { };

	public:

		Session_component(Genode::Env &env, Session_list &sessions,
		                  Output_buffer const &output_buffer)
		:
			_env(env), _output_buffer(output_buffer), _sessions(sessions)
		{
			_sessions.insert(this);
		}

		~Session_component() { _sessions.remove(this); }

		using Session_list::Element::next;

		void notify_client()
		{
			if (!_sigh.valid())
				return;

			Genode::Signal_transmitter(_sigh).submit();
		}

		Genode::Rom_dataspace_capability dataspace() override
		{
			using namespace Genode;

			/* replace dataspace by new one as needed */
			if (!_ram_ds.constructed()
			 || _output_buffer.content_size() > _ram_ds->size()) {

				_ram_ds.construct(_env.ram(), _env.rm(), _output_buffer.content_size());
			}

			char             *dst = _ram_ds->local_addr<char>();
			size_t const dst_size = _ram_ds->size();

			/* fill with content of current evaluation result */
			size_t const copied_len = _output_buffer.export_content(dst, dst_size);

			/* clear remainder of dataspace */
			Genode::memset(dst + copied_len, 0, dst_size - copied_len);

			/* cast RAM into ROM dataspace capability */
			Dataspace_capability ds_cap = static_cap_cast<Dataspace>(_ram_ds->cap());
			return static_cap_cast<Rom_dataspace>(ds_cap);
		}

		void sigh(Genode::Signal_context_capability sigh) override
		{
			_sigh = sigh;
		}
};


class Rom_filter::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env   &_env;
		Output_buffer &_output_buffer;
		Session_list   _sessions { };

	protected:

		Session_component *_create_session(const char *) override
		{
			/*
			 * We ignore the name of the ROM module requested
			 */
			return new (md_alloc()) Session_component(_env, _sessions, _output_buffer);
		}

	public:

		Root(Genode::Env       &env,
		     Output_buffer     &output_buffer,
		     Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _output_buffer(output_buffer)
		{ }

		void notify_clients()
		{
			for (Session_component *s = _sessions.first(); s; s = s->next())
				s->notify_client();
		}
};


struct Rom_filter::Main : Input_rom_registry::Input_rom_changed_fn,
                          Output_buffer
{
	Genode::Env &_env;

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Input_rom_registry _input_rom_registry { _env, _heap, *this };

	Genode::Constructible<Genode::Attached_ram_dataspace> _xml_ds { };

	size_t _xml_output_len = 0;

	void _evaluate_node(Xml_node node, Xml_generator &xml);
	void _evaluate();

	Root _root = { _env, *this, _sliced_heap };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	Genode::Signal_handler<Main> _config_handler =
		{ _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();

		_verbose = _config.xml().attribute_value("verbose", false);

		/*
		 * Create buffer for generated XML data
		 */
		Genode::Number_of_bytes xml_ds_size = 4096;

		xml_ds_size = _config.xml().attribute_value("buffer", xml_ds_size);

		if (!_xml_ds.constructed() || xml_ds_size != _xml_ds->size())
			_xml_ds.construct(_env.ram(), _env.rm(), xml_ds_size);

		/*
		 * Obtain inputs
		 */
		try {
			_input_rom_registry.update_config(_config.xml());
		} catch (Xml_node::Nonexistent_sub_node) { }

		/*
		 * Generate output
		 */
		_evaluate();
	}

	/**
	 * Input_rom_registry::Input_rom_changed_fn interface
	 *
	 * Called each time one of the input ROM modules changes.
	 */
	void input_rom_changed() override
	{
		_evaluate();
	}

	/**
	 * Output_buffer interface
	 */
	size_t content_size() const override { return _xml_output_len; }

	/**
	 * Output_buffer interface
	 */
	size_t export_content(char *dst, size_t dst_len) const override
	{
		size_t const len = Genode::min(dst_len, _xml_output_len);
		Genode::memcpy(dst, _xml_ds->local_addr<char>(), len);
		return len;
	}

	Main(Genode::Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Rom_filter::Main::_evaluate_node(Xml_node node, Xml_generator &xml)
{
	auto process_output_sub_node = [&] (Xml_node node) {

		if (node.has_type("if")) {

			/*
			 * Check condition
			 */
			bool condition_satisfied = false;

			if (node.has_sub_node("has_value")) {

				Xml_node const has_value_node = node.sub_node("has_value");

				Input_name const input_name =
					has_value_node.attribute_value("input", Input_name());

				Input_value const expected_input_value =
					has_value_node.attribute_value("value", Input_value());

				try {
					Input_value const input_value =
						_input_rom_registry.query_value(_config.xml(), input_name);

					if (input_value == expected_input_value)
						condition_satisfied = true;
				}
				catch (Input_rom_registry::Nonexistent_input_value) {
					if (_verbose)
						Genode::warning("could not obtain input value for input ", input_name);
				}
			}

			if (condition_satisfied) {
				if (node.has_sub_node("then"))
					_evaluate_node(node.sub_node("then"), xml);
			} else {
				if (node.has_sub_node("else"))
					_evaluate_node(node.sub_node("else"), xml);
			}
		} else

		if (node.has_type("attribute")) {

			typedef Genode::String<128> String;

			/* assign input value to attribute value */
			if (node.has_attribute("input")) {

				Input_name const input_name =
					node.attribute_value("input", Input_name());
				try {
					Input_value const input_value =
						_input_rom_registry.query_value(_config.xml(), input_name);

					xml.attribute(node.attribute_value("name", String()).string(),
					              input_value);
				}
				catch (Input_rom_registry::Nonexistent_input_value) {
					if (_verbose)
						Genode::warning("could not obtain input value for input ", input_name);
				}
			}

			/* assign fixed attribute value */
			else {
				xml.attribute(node.attribute_value("name",  String()).string(),
				              node.attribute_value("value", String()).string());
			}
		} else

		if (node.has_type("inline")) {

			node.with_raw_content([&] (char const *src, size_t len) {

				/*
				 * The 'Xml_generator::append' method puts the content at a
				 * fresh line, and also adds a newline before the closing tag.
				 * We strip eventual newlines from the '<inline>' node content
				 * to avoid double newlines in the output.
				 */

				/* remove leading newline */
				if (len > 0 && src[0] == '\n') {
					src++;
					len--;
				}

				/* remove trailing whilespace including newlines */
				for (; len > 0 && Genode::is_whitespace(src[len - 1]); len--);

				xml.append(src, len);
			});
		} else

		if (node.has_type("input")) {

			Input_name const input_name =
				node.attribute_value("name", Input_name());

			try {
				_input_rom_registry.gen_xml(input_name, xml); }
			catch (...) { }
		}
	};

	node.for_each_sub_node(process_output_sub_node);
}


void Rom_filter::Main::_evaluate()
{
	try {
		Xml_node output = _config.xml().sub_node("output");

		if (!output.has_attribute("node")) {
			Genode::error("missing 'node' attribute in '<output>' node");
			return;
		}

		Node_type_name const node_type =
			output.attribute_value("node", Node_type_name(""));

		/*
		 * Generate output, expand dataspace on demand
		 */
		enum { UPGRADE = 4096, NUM_ATTEMPTS = ~0L };
		Genode::retry<Xml_generator::Buffer_exceeded>(
			[&] () {
				Xml_generator xml(_xml_ds->local_addr<char>(),
				                  _xml_ds->size(), node_type.string(),
				                  [&] () { _evaluate_node(output, xml); });
				_xml_output_len = xml.used();
			},
			[&] () {
				_xml_ds.construct(_env.ram(), _env.rm(), _xml_ds->size() + UPGRADE);
			},
			NUM_ATTEMPTS);

	} catch (Xml_node::Nonexistent_sub_node) { }

	_root.notify_clients();
}


void Component::construct(Genode::Env &env) { static Rom_filter::Main main(env); }

