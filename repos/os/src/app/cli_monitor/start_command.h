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

struct Start_command : Command
{
	typedef Genode::Xml_node                  Xml_node;
	typedef Genode::Signal_context_capability Signal_context_capability;

	Ram                      &_ram;
	Child_registry           &_children;
	Genode::Cap_session      &_cap;
	Xml_node                  _config;
	Process_arg_registry     &_process_args;
	List<Argument>            _arguments;
	Signal_context_capability _yield_response_sigh_cap;

	Start_command(Ram &ram, Genode::Cap_session &cap, Child_registry &children,
	              Xml_node config, Process_arg_registry &process_args,
	              Signal_context_capability yield_response_sigh_cap)
	:
		Command("start", "create new subsystem"),
		_ram(ram), _children(children), _cap(cap), _config(config),
		_process_args(process_args),
		_yield_response_sigh_cap(yield_response_sigh_cap)
	{
		/* scan config for possible subsystem arguments */
		try {
			Xml_node node = _config.sub_node("subsystem");
			for (;; node = node.next("subsystem")) {

				char name[Parameter::NAME_MAX_LEN];
				try { node.attribute("name").value(name, sizeof(name)); }
				catch (Xml_node::Nonexistent_attribute) {
					PWRN("Missing name in '<subsystem>' configuration");
					continue;
				}

				char   const *prefix     = "config: ";
				size_t const  prefix_len = strlen(prefix);

				char help[Parameter::SHORT_HELP_MAX_LEN + prefix_len];
				strncpy(help, prefix, ~0);
				try { node.attribute("help").value(help + prefix_len,
				                                   sizeof(help) - prefix_len); }
				catch (Xml_node::Nonexistent_attribute) {
					PWRN("Missing help in '<subsystem>' configuration");
					continue;
				}

				_arguments.insert(new Argument(name, help));
			}
		} catch (Xml_node::Nonexistent_sub_node) { /* end of list */ }

		add_parameter(new Parameter("--count",     Parameter::NUMBER, "number of instances"));
		add_parameter(new Parameter("--ram",       Parameter::NUMBER, "initial RAM quota"));
		add_parameter(new Parameter("--ram-limit", Parameter::NUMBER, "limit for expanding RAM quota"));
		add_parameter(new Parameter("--verbose",   Parameter::VOID,   "show diagnostics"));
	}

	/**
	 * Lookup subsystem in config
	 */
	Xml_node _subsystem_node(char const *name)
	{
		Xml_node node = _config.sub_node("subsystem");
		for (;; node = node.next("subsystem")) {
			if (node.attribute("name").has_value(name))
				return node;
		}
	}

	void execute(Command_line &cmd, Terminal::Session &terminal)
	{
		size_t count = 1;
		Genode::Number_of_bytes ram = 0;
		Genode::Number_of_bytes ram_limit = 0;

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

		/* check if a configuration for the subsystem exists */
		try { _subsystem_node(name); }
		catch (Xml_node::Nonexistent_sub_node) {
			tprintf(terminal, "Error: no configuration for \"%s\"\n", name);
			return;
		}

		/* read default RAM quota from config */
		try {
			Xml_node rsc = _subsystem_node(name).sub_node("resource");
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
			Xml_node bin = _subsystem_node(name).sub_node("binary");
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
					Child(_ram, label, binary_name, _cap, ram, ram_limit,
					      _yield_response_sigh_cap);
			}
			catch (Genode::Rom_connection::Rom_connection_failed) {
				tprintf(terminal, "Error: could not obtain ROM module \"%s\"\n",
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
				Xml_node config_node = _subsystem_node(name).sub_node("config");
				child->configure(config_node.addr(), config_node.size());
				if (verbose)
					tprintf(terminal, "     config: inline\n");
			} catch (...) {
				if (verbose)
					tprintf(terminal, "     config: none\n");
			}

			_process_args.list.insert(&child->argument);
			_children.insert(child);
			child->start();
		}
	}

	List<Argument> &arguments() { return _arguments; }
};

#endif /* _START_COMMAND_H_ */
