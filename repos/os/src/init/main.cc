/*
 * \brief  Init component
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/sandbox.h>
#include <os/reporter.h>

namespace Init {

	using namespace Genode;

	struct Main;
}


struct Init::Main : Sandbox::State_handler
{
	Env &_env;

	Sandbox _sandbox { _env, *this };

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_resource_avail() { }

	Signal_handler<Main> _resource_avail_handler {
		_env.ep(), *this, &Main::_handle_resource_avail };

	Constructible<Reporter> _reporter { };

	size_t _report_buffer_size = 0;

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		bool reporter_enabled = false;
		config.with_sub_node("report", [&] (Xml_node report) {

			reporter_enabled = true;

			/* (re-)construct reporter whenever the buffer size is changed */
			Number_of_bytes const buffer_size =
				report.attribute_value("buffer", Number_of_bytes(4096));

			if (buffer_size != _report_buffer_size || !_reporter.constructed()) {
				_report_buffer_size = buffer_size;
				_reporter.construct(_env, "state", "state", _report_buffer_size);
			}
		});

		if (_reporter.constructed())
			_reporter->enabled(reporter_enabled);

		_sandbox.apply_config(config);
	}

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	/**
	 * Sandbox::State_handler interface
	 */
	void handle_sandbox_state() override
	{
		try {
			Reporter::Xml_generator xml(*_reporter, [&] () {
				_sandbox.generate_state_report(xml); });
		}
		catch (Xml_generator::Buffer_exceeded) {

			error("state report exceeds maximum size");

			/* try to reflect the error condition as state report */
			try {
				Reporter::Xml_generator xml(*_reporter, [&] () {
					xml.attribute("error", "report buffer exceeded"); });
			}
			catch (...) { }
		}
	}

	Main(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		/* prevent init to block for resource upgrades (never satisfied by core) */
		_env.parent().resource_avail_sigh(_resource_avail_handler);

		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Init::Main main(env); }

