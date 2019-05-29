/*
 * \brief  Component starting noux in a sub-init to execute a specific command
 * \author Sid Hussmann
 * \date   2019-05-11
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Copyright (C) 2019 gapfruit AG
 */


/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* local includes */

namespace Exec_terminal {

	class Main;

	using namespace Genode;
}

class Exec_terminal::Main
{
	private:

		Env&                   _env;
		Attached_rom_dataspace _exec_terminal_config           { _env, "exec_terminal.config" };
		Signal_handler<Main>   _exec_terminal_config_handler   { _env.ep(), *this, &Main::_handle_exec_terminal_config };
		Expanding_reporter     _init_config_reporter           { _env, "config", "config" };

		unsigned int           _version                        { 0 };

		void _handle_exec_terminal_config();

	public:

		Main(Env& env) :
			_env{env}
		{
			_exec_terminal_config.sigh(_exec_terminal_config_handler);
			_handle_exec_terminal_config();
		}

		virtual ~Main() = default;
};


void Exec_terminal::Main::_handle_exec_terminal_config()
{
	_exec_terminal_config.update();

	const Xml_node cfg = _exec_terminal_config.xml();

	Genode::log(cfg);
	if (!cfg.has_type("empty")) {
		if (cfg.has_attribute("exit")) {
			_init_config_reporter.generate([&] (Xml_generator& xml) {
				xml.node("empty");
			});
		} else {

			_init_config_reporter.generate([&] (Xml_generator& xml) {
				xml.node("parent-provides",[&] () {
					xml.node("service",[&] () { xml.attribute("name", "CPU"); });
					xml.node("service",[&] () { xml.attribute("name", "File_system"); });
					xml.node("service",[&] () { xml.attribute("name", "LOG"); });
					xml.node("service",[&] () { xml.attribute("name", "PD"); });
					xml.node("service",[&] () { xml.attribute("name", "RM"); });
					xml.node("service",[&] () { xml.attribute("name", "ROM"); });
					xml.node("service",[&] () { xml.attribute("name", "Report"); });
					xml.node("service",[&] () { xml.attribute("name", "Terminal"); });
					xml.node("service",[&] () { xml.attribute("name", "Timer"); });
				});

				xml.node("start",[&] () {
					xml.attribute("name", "noux");
					xml.attribute("caps", "500");
					xml.attribute("version", ++_version);
					xml.node("resource",[&] () { xml.attribute("name", "RAM"); xml.attribute("quantum", "64M"); });
					xml.node("config",[&] () {
						xml.node("fstab",[&] () {
							xml.node("tar",[&] () { xml.attribute("name", "bash.tar"); });
							xml.node("tar",[&] () { xml.attribute("name", "coreutils-minimal.tar"); });
							xml.node("tar",[&] () { xml.attribute("name", "vim-minimal.tar"); });
							xml.node("dir",[&] () {
								xml.attribute("name", "rw");
								xml.node("fs",[&] () { xml.attribute("label", "rw"); });
							});
							xml.node("dir",[&] () {
								xml.attribute("name", "tmp");
								xml.node("ram",[&] () { });
							});
						});
						xml.node("start",[&] () {
							xml.attribute("name", "/bin/bash");
							xml.node("env",[&] () {
								xml.attribute("name", "TERM");
								xml.attribute("value", "screen");
							});
							xml.node("env",[&] () {
								xml.attribute("name", "HOME");
								xml.attribute("value", "/");
							});
							xml.node("env",[&] () {
								xml.attribute("name", "IGNOREOF");
								xml.attribute("value", "3");
							});
							if (cfg.has_attribute("command")) {
								Genode::String<128> cmd;
								cfg.attribute_value("command", &cmd);
								if (cmd.valid()) {
									xml.node("arg",[&] () {
										xml.attribute("value", "-c");
									});
									xml.node("arg",[&] () {
										// FIXME appending " ; true" is done to force bash to fork.
										// noux will fail otherwise. This invalidates any exit codes.
										xml.attribute("value", Genode::String<136>(cmd, " ; true"));
									});
								}
							} else {
								xml.node("env",[&] () {
									xml.attribute("name", "PS1");
									xml.attribute("value", "noux@$PWD> ");
								});
							}
						});
					});
					xml.node("route",[&] () {
						xml.node("service",[&] () { xml.attribute("name", "CPU"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "File_system"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "LOG"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "PD"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "RM"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "ROM"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "Terminal"); xml.node("parent",[&] () {}); });
						xml.node("service",[&] () { xml.attribute("name", "Timer"); xml.node("parent",[&] () {}); });
					});
				});
			});
		}
	}
}


void Component::construct(Genode::Env &env) {
	static Exec_terminal::Main main(env);
}
