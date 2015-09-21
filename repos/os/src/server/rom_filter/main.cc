/*
 * \brief  ROM server that generates a ROM depending on other ROMs
 * \author Norman Feske
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/volatile_object.h>
#include <util/arg_string.h>
#include <util/xml_generator.h>
#include <base/heap.h>
#include <base/env.h>
#include <root/component.h>

/* local includes */
#include "input_rom_registry.h"

namespace Rom_filter {
	using Server::Entrypoint;
	using Genode::Rpc_object;
	using Genode::Sliced_heap;
	using Genode::env;
	using Genode::Lazy_volatile_object;
	using Genode::Xml_generator;
	using Genode::size_t;

	class  Output_buffer;
	class  Session_component;
	class  Root;
	struct Main;

	typedef Genode::List<Session_component> Session_list;
}


/**
 * Interface used by the sessions to obtain the XML output data
 */
struct Rom_filter::Output_buffer
{
	virtual size_t content_size() const = 0;
	virtual size_t export_content(char *dst, size_t dst_len) const = 0;
};


class Rom_filter::Session_component : public Rpc_object<Genode::Rom_session>,
                                      public Session_list::Element
{
	private:

		Signal_context_capability _sigh;

		Output_buffer const &_output_buffer;

		Session_list &_sessions;

		Lazy_volatile_object<Genode::Attached_ram_dataspace> _ram_ds;

	public:

		Session_component(Session_list &sessions, Output_buffer const &output_buffer)
		:
			_output_buffer(output_buffer), _sessions(sessions)
		{
			_sessions.insert(this);
		}

		~Session_component() { _sessions.remove(this); }

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
			if (!_ram_ds.is_constructed()
			 || _output_buffer.content_size() > _ram_ds->size()) {

				_ram_ds.construct(env()->ram_session(), _output_buffer.content_size());
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

		Output_buffer &_output_buffer;
		Session_list   _sessions;

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * We ignore the name of the ROM module requested
			 */
			return new (md_alloc()) Session_component(_sessions, _output_buffer);
		}

	public:

		Root(Entrypoint &ep, Output_buffer &output_buffer,
		     Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_output_buffer(output_buffer)
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
	Entrypoint &_ep;

	Sliced_heap _sliced_heap = { env()->ram_session(), env()->rm_session() };

	Input_rom_registry _input_rom_registry { *env()->heap(), _ep, *this };

	Genode::Lazy_volatile_object<Genode::Attached_ram_dataspace> _xml_ds;

	size_t _xml_output_len = 0;

	void _evaluate_node(Xml_node node, Xml_generator &xml);
	void _evaluate();

	Root _root = { _ep, *this, _sliced_heap };

	Genode::Signal_rpc_member<Main> _config_dispatcher =
		{ _ep, *this, &Main::_handle_config };

	void _handle_config(unsigned)
	{
		Genode::config()->reload();

		/*
		 * Create buffer for generated XML data
		 */
		Genode::Number_of_bytes xml_ds_size = 4096;

		xml_ds_size = Genode::config()->xml_node().attribute_value("buffer", xml_ds_size);

		if (!_xml_ds.is_constructed() || xml_ds_size != _xml_ds->size())
			_xml_ds.construct(env()->ram_session(), xml_ds_size);

		/*
		 * Obtain inputs
		 */
		try {
			_input_rom_registry.update_config(Genode::config()->xml_node());
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
	size_t content_size() const override
	{
		return _xml_output_len;
	}

	/**
	 * Output_buffer interface
	 */
	size_t export_content(char *dst, size_t dst_len) const
	{
		size_t const len = Genode::min(dst_len, _xml_output_len);
		Genode::memcpy(dst, _xml_ds->local_addr<char>(), len);
		return len;
	}

	Main(Entrypoint &ep) : _ep(ep)
	{
		env()->parent()->announce(_ep.manage(_root));

		_handle_config(0);
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
					Xml_node config = Genode::config()->xml_node();

					Input_value const input_value =
						_input_rom_registry.query_value(config, input_name);

					if (input_value == expected_input_value)
						condition_satisfied = true;
				}
				catch (Input_rom_registry::Nonexistent_input_value) {
					PWRN("could not obtain input value for input %s", input_name.string());
				}
			}

			if (condition_satisfied) {
				if (node.has_sub_node("then"))
					_evaluate_node(node.sub_node("then"), xml);
			} else {
				if (node.has_sub_node("else"))
					_evaluate_node(node.sub_node("else"), xml);
			}
		}

		if (node.has_type("inline")) {
			char const *src     = node.content_base();
			size_t      src_len = node.content_size();

			/*
			 * The 'Xml_generator::append' method puts the content at a fresh
			 * line, and also adds a newline before the closing tag. We strip
			 * eventual newlines from the '<inline>' node content to avoid
			 * double newlines in the output.
			 */

			/* remove leading newline */
			if (src_len > 0 && src[0] == '\n') {
				src++;
				src_len--;
			}

			/* remove trailing whilespace including newlines */
			for (; src_len > 0 && Genode::is_whitespace(src[src_len - 1]); src_len--);

			xml.append(src, src_len);
		}
	};

	node.for_each_sub_node(process_output_sub_node);
}


void Rom_filter::Main::_evaluate()
{
	try {
		Xml_node output = Genode::config()->xml_node().sub_node("output");

		if (!output.has_attribute("node")) {
			PERR("missing 'node' attribute in '<output>' node");
			return;
		}

		Node_type_name const node_type =
			output.attribute_value("node", Node_type_name(""));

		/* generate output */
		Xml_generator xml(_xml_ds->local_addr<char>(),
		                  _xml_ds->size(), node_type.string(),
		                  [&] () { _evaluate_node(output, xml); });

		_xml_output_len = xml.used();

	} catch (Xml_node::Nonexistent_sub_node) { }

	_root.notify_clients();
}


namespace Server {

	char const *name() { return "conditional_rom_ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Rom_filter::Main main(ep);
	}
}
