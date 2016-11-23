/*
 * \brief  Start command
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _START_COMMAND_H_
#define _START_COMMAND_H_

/* Genode includes */
#include <util/xml_node.h>

/* local includes */
#include <subsystem_config_registry.h>

class Start_command : public Command
{
	private:

		typedef Genode::Xml_node                  Xml_node;
		typedef Genode::Signal_context_capability Signal_context_capability;
		typedef Genode::Dataspace_capability      Dataspace_capability;

		Ram                           &_ram;
		Child_registry                &_children;
		Genode::Pd_session            &_pd;
		Genode::Ram_session           &_ref_ram;
		Genode::Ram_session_capability _ref_ram_cap;
		Genode::Region_map            &_local_rm;
		Subsystem_config_registry     &_subsystem_configs;
		List<Argument>                 _arguments;
		Signal_context_capability      _yield_response_sigh_cap;
		Signal_context_capability      _exit_sig_cap;

		void _execute_subsystem(char const *name, Command_line &cmd,
		                        Terminal::Session &terminal,
		                        Genode::Xml_node subsystem_node)
		{
			size_t count = 1;
			Genode::Number_of_bytes ram = 0;
			Genode::Number_of_bytes ram_limit = 0;

			/* read default RAM quota from config */
			try {
				Xml_node rsc = subsystem_node.sub_node("resource");
				for (;; rsc = rsc.next("resource")) {
					if (rsc.attribute("name").has_value("RAM")) {
						rsc.attribute("quantum").value(&ram);

						if (rsc.has_attribute("limit"))
							rsc.attribute("limit").value(&ram_limit);
						break;
					}
				}
			} catch (...) { }

			cmd.parameter("--count",     count);
			cmd.parameter("--ram",       ram);
			cmd.parameter("--ram-limit", ram_limit);

			/* acount for cli_monitor local metadata */
			size_t preserve_ram = 100*1024;
			if (count * (ram + preserve_ram) > Genode::env()->ram_session()->avail()) {
				tprintf(terminal, "Error: RAM quota exceeds available quota\n");
				return;
			}

			bool const verbose = cmd.parameter_exists("--verbose");

			/*
			 * Determine binary name
			 *
			 * Use subsystem name by default, override with '<binary>' declaration.
			 */
			char binary_name[128];
			strncpy(binary_name, name, sizeof(binary_name));
			try {
				Xml_node bin = subsystem_node.sub_node("binary");
				bin.attribute("name").value(binary_name, sizeof(binary_name));
			} catch (...) { }

			for (unsigned i = 0; i < count; i++) {

				/* generate unique child name */
				char label[Child_registry::CHILD_NAME_MAX_LEN];
				_children.unique_child_name(name, label, sizeof(label));

				tprintf(terminal, "starting new subsystem '%s'\n", label);

				if (verbose) {
					tprintf(terminal, "  RAM quota: ");
					tprint_bytes(terminal, ram);
					tprintf(terminal,"\n");
					if (ram_limit) {
						tprintf(terminal, "  RAM limit: ");
						tprint_bytes(terminal, ram_limit);
						tprintf(terminal,"\n");
					}
					tprintf(terminal, "     binary: %s\n", binary_name);
				}

				Child *child = 0;
				try {
					child = new (Genode::env()->heap())
						Child(_ram, label, binary_name, _pd, _ref_ram, _ref_ram_cap,
						      _local_rm, ram, ram_limit,
						      _yield_response_sigh_cap, _exit_sig_cap);
				}
				catch (Genode::Parent::Service_denied) {
					tprintf(terminal, "Error: could not start child  \"%s\"\n",
					        binary_name);
					return;
				}
				catch (Child::Quota_exceeded) {
					tprintf(terminal, "Error: insufficient memory, need ");
					tprint_bytes(terminal, ram + Child::DONATED_RAM_QUOTA);
					tprintf(terminal, ", have ");
					tprint_bytes(terminal, Genode::env()->ram_session()->avail());
					tprintf(terminal, "\n");
					return;
				}
				catch (Genode::Allocator::Out_of_memory) {
					tprintf(terminal, "Error: could not allocate meta data, out of memory\n");
					return;
				}

				/* configure child */
				try {
					Xml_node config_node = subsystem_node.sub_node("config");
					child->configure(config_node.addr(), config_node.size());
					if (verbose)
						tprintf(terminal, "     config: inline\n");
				} catch (...) {
					if (verbose)
						tprintf(terminal, "     config: none\n");
				}

				_children.insert(child);
				child->start();
			}
		}

	public:

		Start_command(Ram                           &ram,
		              Genode::Pd_session            &pd,
		              Genode::Ram_session           &ref_ram,
		              Genode::Ram_session_capability ref_ram_cap,
		              Genode::Region_map            &local_rm,
		              Child_registry                &children,
		              Subsystem_config_registry     &subsustem_configs,
		              Signal_context_capability      yield_response_sigh_cap,
		              Signal_context_capability      exit_sig_cap)
		:
			Command("start", "create new subsystem"),
			_ram(ram), _children(children), _pd(pd),
			_ref_ram(ref_ram), _ref_ram_cap(ref_ram_cap), _local_rm(local_rm),
			_subsystem_configs(subsustem_configs),
			_yield_response_sigh_cap(yield_response_sigh_cap),
			_exit_sig_cap(exit_sig_cap)
		{
			add_parameter(new Parameter("--count",     Parameter::NUMBER, "number of instances"));
			add_parameter(new Parameter("--ram",       Parameter::NUMBER, "initial RAM quota"));
			add_parameter(new Parameter("--ram-limit", Parameter::NUMBER, "limit for expanding RAM quota"));
			add_parameter(new Parameter("--verbose",   Parameter::VOID,   "show diagnostics"));
		}

		void _for_each_argument(Argument_fn const &fn) const override
		{
			/* functor for processing a subsystem configuration */
			auto process_subsystem_config_fn = [&] (Genode::Xml_node node) {

				char name[Parameter::Name::size()];
				try { node.attribute("name").value(name, sizeof(name)); }
				catch (Xml_node::Nonexistent_attribute) {
					PWRN("Missing name in '<subsystem>' configuration");
					return;
				}

				char   const *prefix     = "config: ";
				size_t const  prefix_len = strlen(prefix);

				char help[Parameter::Short_help::size() + prefix_len];
				strncpy(help, prefix, ~0);
				try { node.attribute("help").value(help + prefix_len,
				                                   sizeof(help) - prefix_len); }
				catch (Xml_node::Nonexistent_attribute) {
					PWRN("Missing help in '<subsystem>' configuration");
					return;
				}

				Argument arg(name, help);
				fn(arg);
			};

			/* scan subsystem config registry for possible subsystem arguments */
			_subsystem_configs.for_each_config(process_subsystem_config_fn);
		}

		void execute(Command_line &cmd, Terminal::Session &terminal)
		{
			char name[128];
			name[0] = 0;
			if (cmd.argument(0, name, sizeof(name)) == false) {
				tprintf(terminal, "Error: no configuration name specified\n");
				return;
			}

			char buf[128];
			if (cmd.argument(1, buf, sizeof(buf))) {
				tprintf(terminal, "Error: unexpected argument \"%s\"\n", buf);
				return;
			}

			try {
				_subsystem_configs.for_config(name, [&] (Genode::Xml_node node)
				{
					_execute_subsystem(name, cmd, terminal, node);
				});

			} catch (Subsystem_config_registry::Nonexistent_subsystem_config) {
				tprintf(terminal, "Error: no configuration for \"%s\"\n", name);
			}
		}

		List<Argument> &arguments() { return _arguments; }
};

#endif /* _START_COMMAND_H_ */
