/*
 * \brief  Clipboard used for copy and paste between domains
 * \author Norman Feske
 * \date   2015-09-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <os/attached_rom_dataspace.h>
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
	Registry(Module::Read_policy  const &read_policy,
	         Module::Write_policy const &write_policy)
	:
		module("clipboard", read_policy, write_policy)
	{ }
};


struct Clipboard::Main : Rom::Module::Read_policy, Rom::Module::Write_policy
{
	Genode::Env &_env;

	Genode::Sliced_heap _sliced_heap = { Genode::env()->ram_session(),
	                                     Genode::env()->rm_session() };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	bool _verbose_config()
	{
		char const *attr = "verbose";
		return _config.xml().attribute_value(attr, false);
	}

	bool verbose = _verbose_config();

	typedef Genode::String<100> Domain;

	Genode::Attached_rom_dataspace _focus_ds { "focus" };

	Genode::Signal_handler<Main> _focus_handler =
		{ _env.ep(), *this, &Main::_handle_focus };

	Domain _focused_domain;

	/**
	 * Handle the change of the current nitpicker focus
	 *
	 * We only accept reports from the currently focused domain.
	 */
	void _handle_focus()
	{
		_focus_ds.update();

		_focused_domain = Domain();

		try {
			Genode::Xml_node focus(_focus_ds.local_addr<char>(), _focus_ds.size());

			if (focus.attribute_value("active", false))
				_focused_domain = focus.attribute_value("domain", Domain());

		} catch (...) { }
	}

	Domain _domain(Genode::Session_label const &label) const
	{
		using namespace Genode;

		try {
			return Session_policy(label, _config.xml()).attribute_value("domain", Domain());
		} catch (Session_policy::No_policy_defined) { }

		return Domain();
	}

	Domain _domain(Rom::Reader const &reader) const
	{
		Rom::Session_component const &rom_session =
			static_cast<Rom::Session_component const &>(reader);

		return _domain(rom_session.label());
	}

	Domain _domain(Rom::Writer const &writer) const
	{
		Report::Session_component const &report_session =
			static_cast<Report::Session_component const &>(writer);

		return _domain(report_session.label());
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

	/**
	 * Rom::Module::Read_policy interface
	 */
	bool read_permitted(Rom::Module const &module,
	                    Rom::Writer const &writer,
	                    Rom::Reader const &reader) const override
	{
		Domain const from_domain = _domain(writer);
		Domain const to_domain   = _domain(reader);

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
		if (_focused_domain.valid() && _domain(writer) == _focused_domain)
			return true;

		warning("unexpected attempt by '", writer.label(), "' "
		        "to write to '", module.name(), "'");

		return false;
	}

	Rom::Registry _rom_registry { *this, *this };

	Report::Root report_root = { _env, _sliced_heap, _rom_registry, verbose };
	Rom   ::Root    rom_root = { _env, _sliced_heap, _rom_registry };

	Main(Genode::Env &env) : _env(env)
	{
		_focus_ds.sigh(_focus_handler);

		env.parent().announce(env.ep().manage(report_root));
		env.parent().announce(env.ep().manage(rom_root));
	}
};


/***************
 ** Component **
 ***************/

namespace Component {

	Genode::size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Genode::Env &env) { static Clipboard::Main main(env); }
}
