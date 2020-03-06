/*
 * \brief  Component starting bash in a sub-init to execute a specific command
 * \author Sid Hussmann
 * \author Norman Feske
 * \date   2019-05-11
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

namespace Exec_terminal {

	class Main;

	using namespace Genode;
}


class Exec_terminal::Main
{
	private:

		Env&                   _env;
		Attached_rom_dataspace _config               { _env, "exec_terminal.config" };
		Signal_handler<Main>   _config_handler       { _env.ep(), *this, &Main::_handle_config };
		Expanding_reporter     _init_config_reporter { _env, "config", "config" };
		unsigned int           _version              { 0 };

		void _handle_config();

		void _gen_init_config(Xml_generator &, Xml_node const &config) const;

		void _gen_sub_init_config(Xml_generator &, Xml_node const &config) const;

		static void _gen_service_node(Xml_generator &xml, char const *name)
		{
			xml.node("service",[&] () { xml.attribute("name", name); });
		}

		static void _gen_parent_provides(Xml_generator &xml)
		{
			xml.node("parent-provides",[&] () {
				_gen_service_node(xml, "CPU");
				_gen_service_node(xml, "File_system");
				_gen_service_node(xml, "LOG");
				_gen_service_node(xml, "PD");
				_gen_service_node(xml, "RM");
				_gen_service_node(xml, "ROM");
				_gen_service_node(xml, "Report");
				_gen_service_node(xml, "Terminal");
				_gen_service_node(xml, "Timer");
			});
		}

	public:

		Main(Env& env) : _env(env)
		{
			_config.sigh(_config_handler);
			_handle_config();
		}
};


void Exec_terminal::Main::_handle_config()
{
	_config.update();

	Xml_node const config = _config.xml();

	log(config);
	if (config.has_type("empty"))
		return;

	_version++;

	_init_config_reporter.generate([&] (Xml_generator &xml) {

		if (config.has_attribute("exit"))
			xml.node("empty");
		else
			_gen_init_config(xml, config);
	});
}


void Exec_terminal::Main::_gen_init_config(Xml_generator &xml, Xml_node const &config) const
{
	_gen_parent_provides(xml);

	xml.node("start",[&] () {
		xml.attribute("name",    "init");
		xml.attribute("caps",    900);
		xml.attribute("version", _version);

		xml.node("resource",[&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", "70M"); });

		xml.node("config", [&] () {
			_gen_sub_init_config(xml, config); });

		xml.node("route",[&] () {
			xml.node("any-service",[&] () {
				xml.node("parent",[&] () { }); }); });
	});
}


void Exec_terminal::Main::_gen_sub_init_config(Xml_generator &xml, Xml_node const &config) const
{
	xml.attribute("verbose", "no");

	_gen_parent_provides(xml);

	auto gen_parent_route = [&] (auto name) {
		xml.node("service",[&] () {
			xml.attribute("name", name);
			xml.node("parent",[&] () {}); }); };

	auto gen_ram = [&] (auto ram) {
		xml.node("resource",[&] () {
			xml.attribute("name", "RAM");
			xml.attribute("quantum", ram); }); };

	auto gen_provides_service = [&] (auto name) {
		xml.node("provides", [&] () {
			xml.node("service", [&] () {
				xml.attribute("name", name); }); }); };

	xml.node("start",[&] () {
		xml.attribute("name", "vfs");
		xml.attribute("caps", 120);
		gen_ram("32M");
		gen_provides_service("File_system");
		xml.node("config",[&] () {
			xml.node("vfs",[&] () {
				xml.node("tar",[&] () { xml.attribute("name", "bash.tar"); });
				xml.node("tar",[&] () { xml.attribute("name", "coreutils-minimal.tar"); });
				xml.node("tar",[&] () { xml.attribute("name", "vim-minimal.tar"); });
				xml.node("dir",[&] () {
					xml.attribute("name", "rw");
					xml.node("fs",[&] () { xml.attribute("label", "rw"); });
				});
				xml.node("dir", [&] () {
					xml.attribute("name", "dev");
					xml.node("terminal", [&] () { });
					xml.node("inline", [&] () {
						xml.attribute("name", "rtc");
						xml.append("2018-01-01 00:01");
					});
				});
				xml.node("dir",[&] () {
					xml.attribute("name", "tmp");
					xml.node("ram",[&] () { });
				});
				xml.node("inline", [&] () {
					xml.attribute("name", ".bash_profile");
					xml.append("echo Hello from Genode! > /dev/log");
				});
			});
			xml.node("default-policy", [&] () {
				xml.attribute("root",      "/");
				xml.attribute("writeable", "yes");
			});
		});
		xml.node("route",[&] () {
			gen_parent_route("CPU");
			gen_parent_route("LOG");
			gen_parent_route("PD");
			gen_parent_route("ROM");
			gen_parent_route("File_system");
			gen_parent_route("Terminal");
		});
	});

	auto gen_vfs_route = [&] () {
		xml.node("service",[&] () {
			xml.attribute("name", "File_system");
			xml.node("child",[&] () { xml.attribute("name", "vfs"); }); }); };

	xml.node("start",[&] () {
		xml.attribute("name", "vfs_rom");
		xml.attribute("caps", 100);
		gen_ram("16M");
		gen_provides_service("ROM");
		xml.node("binary",  [&] () { xml.attribute("name", "fs_rom"); });
		xml.node("config",  [&] () { });
		xml.node("route",   [&] () {
			gen_parent_route("CPU");
			gen_parent_route("LOG");
			gen_parent_route("PD");
			gen_parent_route("ROM");
			gen_vfs_route();
		});
	});

	xml.node("start",[&] () {
		xml.attribute("name", "/bin/bash");
		xml.attribute("caps", 500);
		gen_ram("16M");

		/* exit sub init when leaving bash */
		xml.node("exit",[&] () {
			xml.attribute("propagate", "yes"); });

		xml.node("config",[&] () {

			xml.node("libc",[&] () {
				xml.attribute("stdin",  "/dev/terminal");
				xml.attribute("stdout", "/dev/terminal");
				xml.attribute("stderr", "/dev/terminal");
				xml.attribute("rtc",    "/dev/rtc");
			});

			xml.node("vfs",[&] () {
				xml.node("fs",[&] () { xml.attribute("label", "rw"); });
				xml.node("dir", [&] () {
					xml.attribute("name", "dev");
					xml.node("null", [&] () { });
					xml.node("log",  [&] () { });
				});
			});

			auto gen_env = [&] (auto key, auto value) {
				xml.node("env",[&] () {
					xml.attribute("key",   key);
					xml.attribute("value", value); }); };

			auto gen_arg = [&] (auto value) {
				xml.node("arg",[&] () {
					xml.attribute("value", value); }); };

			gen_env("TERM",     "screen");
			gen_env("HOME",     "/");
			gen_env("PATH",     "/bin");
			gen_env("HISTFILE", "");
			gen_env("IGNOREOF", "3");

			gen_arg("/bin/bash");

			if (config.has_attribute("command")) {

				typedef String<128> Command;
				Command const command = config.attribute_value("command", Command());

				if (command.valid()) {
					gen_arg("-c");

					/*
					 * XXX appending " ; true" is done to force bash to fork.
					 * Bash fails to return the proper exit code otherwise.
					 */
					gen_arg(String<200>(command, " ; true"));
				}
			} else {
				gen_env("PS1", "noux@$PWD> ");
				gen_arg("--login");
			}
		});
		xml.node("route",[&] () {
			xml.node("service",[&] () {
				xml.attribute("name", "ROM");
				xml.attribute("label_last", "/bin/bash");
				xml.node("child",[&] () { xml.attribute("name", "vfs_rom"); });
			});
			xml.node("service",[&] () {
				xml.attribute("name", "ROM");
				xml.attribute("label_prefix", "/bin");
				xml.node("child",[&] () { xml.attribute("name", "vfs_rom"); });
			});
			gen_parent_route("CPU");
			gen_parent_route("LOG");
			gen_parent_route("PD");
			gen_parent_route("RM");
			gen_parent_route("ROM");
			gen_parent_route("Timer");
			gen_vfs_route();
		});
	});
}


void Component::construct(Genode::Env &env)
{
	static Exec_terminal::Main main(env);
}
