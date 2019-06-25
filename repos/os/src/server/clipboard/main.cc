/*
 * \brief  Clipboard used for copy and paste between domains
 * \author Norman Feske
 * \date   2015-09-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <report_rom/rom_service.h>
#include <report_rom/report_service.h>

namespace Clipboard { struct Main; }


/**
 * The clipboard uses a single ROM module for all clients
 */
struct Rom::Registry : Rom::Registry_for_reader, Rom::Registry_for_writer
{
	Module module;

	/**
	 * Rom::Registry_for_writer interface
	 */
	Module &lookup(Writer &writer, Module::Name const &) override
	{
		module._register(writer);
		return module;
	}

	void release(Writer &writer, Module &) override
	{
		module._unregister(writer);
	}

	/**
	 * Rom::Registry_for_reader interface
	 */
	Module &lookup(Reader &reader, Module::Name const &) override
	{
		module._register(reader);
		return module;
	}

	void release(Reader &reader, Readable_module &) override
	{
		module._unregister(reader);
	}

	/**
	 * Constructor
	 */
	Registry(Genode::Ram_allocator &ram, Genode::Region_map &rm,
	         Module::Read_policy  const &read_policy,
	         Module::Write_policy const &write_policy)
	:
		module(ram, rm, "clipboard", read_policy, write_policy)
	{ }
};


struct Clipboard::Main : Rom::Module::Read_policy, Rom::Module::Write_policy
{
	Genode::Env &_env;

	Genode::Sliced_heap _sliced_heap = { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	bool _verbose      = false;
	bool _match_labels = false;

	typedef Genode::String<100> Domain;
	typedef Genode::String<100> Label;

	Genode::Attached_rom_dataspace _focus_ds { _env, "focus" };

	Genode::Signal_handler<Main> _config_handler =
		{ _env.ep(), *this, &Main::_handle_config };
	Genode::Signal_handler<Main> _focus_handler =
		{ _env.ep(), *this, &Main::_handle_focus };

	Domain _focused_domain { };
	Label  _focused_label  { };

	/**
	 * Handle configuration changes
	 */
	void _handle_config()
	{
		_config.update();
		_verbose      = _config.xml().attribute_value("verbose",      false);
		_match_labels = _config.xml().attribute_value("match_labels", false);
	}

	/**
	 * Handle the change of the current nitpicker focus
	 *
	 * We only accept reports from the currently focused domain.
	 */
	void _handle_focus()
	{
		_focus_ds.update();

		_focused_domain = Domain();
		_focused_label  = Label();

		Genode::Xml_node const focus = _focus_ds.xml();

		if (focus.attribute_value("active", false)) {
			_focused_domain = focus.attribute_value("domain", Domain());
			_focused_label  = focus.attribute_value("label",  Label());
		}
	}

	Domain _domain(Genode::Session_label const &label) const
	{
		using namespace Genode;

		try {
			return Session_policy(label, _config.xml()).attribute_value("domain", Domain());
		} catch (Session_policy::No_policy_defined) { }

		return Domain();
	}

	Label _label(Rom::Reader const &reader) const
	{
		Rom::Session_component const &rom_session =
			static_cast<Rom::Session_component const &>(reader);

		return rom_session.label();
	}

	Domain _domain(Rom::Reader const &reader) const
	{
		return _domain(_label(reader));
	}

	Label _label(Rom::Writer const &writer) const
	{
		Report::Session_component const &report_session =
			static_cast<Report::Session_component const &>(writer);

		return report_session.label();
	}

	Domain _domain(Rom::Writer const &writer) const
	{
		return _domain(_label(writer));
	}

	bool _flow_defined(Domain const &from, Domain const &to) const
	{
		if (!from.valid() || !to.valid())
			return false;

		/*
		 * Search config for flow node with matching 'from' and 'to'
		 * attributes.
		 */
		bool result = false;
		try {

			auto match_flow = [&] (Genode::Xml_node flow) {
				if (flow.attribute_value("from", Domain()) == from
				 && flow.attribute_value("to",   Domain()) == to)
					result = true; };

			_config.xml().for_each_sub_node("flow", match_flow);

		} catch (Genode::Xml_node::Nonexistent_sub_node) { }

		return result;
	}

	bool _client_label_matches_focus(Genode::Session_label const &label) const
	{
		using namespace Genode;

		/*
		 * The client label has the suffix " -> clipboard". Strip off this
		 * part to make the label comparable with the label of the client's
		 * nitpicker session (which has of course no such suffix).
		 */
		char   const *suffix     = " -> clipboard";
		size_t const  suffix_len = strlen(suffix);

		if (label.length() < suffix_len + 1)
			return false;

		size_t const truncated_label_len = label.length() - suffix_len - 1;

		Session_label const
			truncated_label(Cstring(label.string(), truncated_label_len));

		/*
		 * We accept any subsystem of the client to have the nitpicker focus.
		 * E.g., a multi-window application may have multiple nitpicker
		 * sessions, one for each window. The labels of all of those nitpicker
		 * session share the first part with application's clipboard label.
		 */
		bool const focus_lies_in_client_subsystem =
			!strcmp(_focused_label.string(), truncated_label.string(),
			        truncated_label_len);

		return focus_lies_in_client_subsystem;
	}

	/**
	 * Rom::Module::Read_policy interface
	 */
	bool read_permitted(Rom::Module const &,
	                    Rom::Writer const &writer,
	                    Rom::Reader const &reader) const override
	{
		Domain const from_domain = _domain(writer);
		Domain const to_domain   = _domain(reader);

		if (_match_labels && !_client_label_matches_focus(_label(reader)))
			return false;

		if (from_domain == to_domain)
			return true;

		if (_flow_defined(from_domain, to_domain))
			return true;

		return false;
	}

	/**
	 * Rom::Module::Write_policy interface
	 */
	bool write_permitted(Rom::Module const &module,
	                     Rom::Writer const &writer) const override
	{
		bool const domain_matches =
			_focused_domain.valid() && _domain(writer) == _focused_domain;

		if ((!_match_labels || _client_label_matches_focus(_label(writer))) && domain_matches)
			return true;

		warning("unexpected attempt by '", writer.label(), "' "
		        "to write to '", module.name(), "'");

		return false;
	}

	Rom::Registry _rom_registry { _env.ram(), _env.rm(), *this, *this };

	Report::Root report_root = { _env, _sliced_heap, _rom_registry, _verbose };
	Rom   ::Root    rom_root = { _env, _sliced_heap, _rom_registry };

	Main(Genode::Env &env) : _env(env)
	{
		_handle_config();
		_focus_ds.sigh(_focus_handler);

		_handle_focus();
		env.parent().announce(env.ep().manage(report_root));
		env.parent().announce(env.ep().manage(rom_root));
	}
};


void Component::construct(Genode::Env &env) { static Clipboard::Main main(env); }
