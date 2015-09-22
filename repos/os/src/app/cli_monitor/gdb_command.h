/*
 * \brief  Gdb command
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GDB_COMMAND_H_
#define _GDB_COMMAND_H_

/* Genode includes */
#include <base/elf.h>
#include <os/attached_rom_dataspace.h>
#include <rom_session/connection.h>
#include <util/arg_string.h>
#include <util/xml_node.h>

/* public CLI-monitor includes */
#include <cli_monitor/ram.h>

/* local includes */
#include <child_registry.h>
#include <gdb_prefix.h>
#include <format_util.h>
#include <subsystem_config_registry.h>

class Gdb_command_child : public Child
{
	private:

		Genode::Signal_context_capability _kill_gdb_sig_cap;
		Terminal::Session &_terminal;

		bool _kill_requested;

		void _kill_gdb()
		{
			_kill_requested = true;
			Genode::Signal_transmitter(_kill_gdb_sig_cap).submit();
		}

	public:

		Gdb_command_child(Ram                              &ram,
		                  char                       const *label,
		                  char                       const *binary,
		                  Genode::Cap_session              &cap_session,
		                  Genode::size_t                    ram_quota,
		                  Genode::size_t                    ram_limit,
		                  Genode::Signal_context_capability yield_response_sig_cap,
		                  Genode::Signal_context_capability kill_gdb_sig_cap,
		                  Terminal::Session                &terminal)
		: Child(ram, label, binary, cap_session, ram_quota, ram_limit,
		        yield_response_sig_cap),
		  _kill_gdb_sig_cap(kill_gdb_sig_cap),
		  _terminal(terminal),
		  _kill_requested(false)
		  { }

		bool kill_requested() const { return _kill_requested; }

		/*
		 * Check if GDB-related (Noux) session-requests will be successful.
		 * If not, terminate the subsystem and tell the user about it.
		 */
		Genode::Service *resolve_session_request(const char *service_name,
	                                             const char *args)
	    {
			if (_kill_requested)
				return 0;

			Genode::Service *service =
				Child::resolve_session_request(service_name, args);

			if (!service) {
				tprintf(_terminal, "Error: GDB subsystem session request for service '%s' failed\n",
				        service_name);
				PDBG("session request failed: service_name = %s, args = %s", service_name, args);
				_kill_gdb();
				return 0;
			}

			/* Find out if the session request comes from Noux */

			const char *noux_label_suffix = " -> noux";

			/* Arg::string() needs two extra bytes */
			size_t label_size = strlen(name()) + strlen(noux_label_suffix) + 2;

			char noux_label_buf[label_size];
			snprintf(noux_label_buf, sizeof(noux_label_buf),
			         "%s%s", name(), noux_label_suffix);

			char label_buf[label_size];
			Genode::Arg_string::find_arg(args, "label").string(label_buf,
			                                                   sizeof(label_buf),
			                                                   "");

			if (strcmp(label_buf, noux_label_buf, label_size) == 0) {
				/* Try to create and close the session */
				try {
					Genode::Session_capability s = service->session(args, Genode::Affinity());
					service->close(s);
				} catch (...) {
					tprintf(_terminal, "Error: GDB subsystem session request for service '%s' failed\n",
					        service_name);
					PDBG("session request failed: service_name = %s, args = %s", service_name, args);
					_kill_gdb();
					return 0;
				}
			}

			return service;
	    }

};


class Gdb_command : public Command
{
	private:

		typedef Genode::Xml_node                  Xml_node;
		typedef Genode::Signal_context_capability Signal_context_capability;

		struct Child_configuration_failed {}; /* exception */

		Ram                       &_ram;
		Child_registry            &_children;
		Genode::Cap_session       &_cap;
		Subsystem_config_registry &_subsystem_configs;
		Signal_context_capability  _yield_response_sigh_cap;
		Signal_context_capability  _kill_gdb_sig_cap;

		/**
		 * Generate the config node for the GDB subsystem
		 */
		Xml_node _gdb_config_node(const char *binary_name,
		                          const char *target_config_addr,
		                          const size_t target_config_size,
		                          Genode::Number_of_bytes gdb_ram_preserve,
		                          Terminal::Session &terminal)
		{
			/* local exception types */
			struct Config_buffer_overflow { };
			struct Binary_elf_check_failed { };

			try {
				Genode::Attached_rom_dataspace gdb_command_config_ds("gdb_command_config");

				enum { CONFIG_BUF_SIZE = 4*1024 };
				static char config_buf[CONFIG_BUF_SIZE];

				int config_bytes_written = 0;

				/*
				 * Copy the first part of the config file template
				 */

				Xml_node init_config_node(gdb_command_config_ds.local_addr<const char>(),
					                      gdb_command_config_ds.size());

				Xml_node noux_node = init_config_node.sub_node("start");
				for (;; noux_node = noux_node.next("start"))
					if (noux_node.attribute("name").has_value("noux"))
						break;

				Xml_node noux_config_node = noux_node.sub_node("config");

				/* Genode::strncpy() makes the last character '\0', so we need to add 1 byte */
				size_t bytes_to_copy = (Genode::addr_t)noux_config_node.content_addr() -
					                   (Genode::addr_t)init_config_node.addr() + 1;

				if ((sizeof(config_buf) - config_bytes_written) < bytes_to_copy)
					throw Config_buffer_overflow();

				strncpy(&config_buf[config_bytes_written],
					    init_config_node.addr(),
					    bytes_to_copy);

				/* subtract the byte for '\0' again */
				config_bytes_written += bytes_to_copy - 1;

				/*
				 * Create the GDB arguments for breaking in 'main()'
				 */

				enum { GDB_MAIN_BREAKPOINT_ARGS_BUF_SIZE = 768 };
				static char gdb_main_breakpoint_args_buf[GDB_MAIN_BREAKPOINT_ARGS_BUF_SIZE];

				try {
					Genode::Attached_rom_dataspace binary_rom_ds(binary_name);
					Genode::Elf_binary elf_binary((Genode::addr_t)binary_rom_ds.local_addr<void>());

					if (elf_binary.is_dynamically_linked()) {

						snprintf(gdb_main_breakpoint_args_buf,
					         	 sizeof(gdb_main_breakpoint_args_buf),
					         	 "<arg value=\"-ex\" /><arg value=\"symbol-file /gdb/ld.lib.so\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"b call_program_main\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"c\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"delete 1\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"symbol-file /gdb/%s\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"b main()\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"set solib-search-path /gdb\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"sharedlibrary\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"c\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"delete 2\" />\n",
					          	  binary_name);
					} else {

						snprintf(gdb_main_breakpoint_args_buf,
					         	 sizeof(gdb_main_breakpoint_args_buf),
					         	 "<arg value=\"-ex\" /><arg value=\"symbol-file /gdb/%s\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"b main\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"c\" />\n \
					          	  <arg value=\"-ex\" /><arg value=\"delete 1\" />\n",
					          	  binary_name);
					}
				} catch (...) {
					throw Binary_elf_check_failed();
				}

				/*
				 * Insert the '<start>' node for 'noux'
				 */

				config_bytes_written += snprintf(&config_buf[config_bytes_written],
				                                 sizeof(config_buf) - config_bytes_written, "\n \
					<start name=\"/bin/%sgdb\">\n \
						<arg value=\"/gdb/%s\"/>\n \
						<arg value=\"-ex\" /><arg value=\"set interactive-mode off\" />\n \
						<arg value=\"-ex\" /><arg value=\"directory /gdb/src\" />\n \
						<arg value=\"-ex\" /><arg value=\"target remote /dev/gdb\" />\n \
						%s \
						<arg value=\"-ex\" /><arg value=\"set interactive-mode auto\" />\n \
					</start>",
					gdb_prefix,
					binary_name,
					gdb_main_breakpoint_args_buf);

				/*
				 * Copy the second part of the config file template
				 */

				Xml_node gdb_monitor_node = noux_node.next("start");

				/* Genode::strncpy() makes the last character '\0', so we need to add 1 byte */
				bytes_to_copy = (Genode::addr_t)gdb_monitor_node.content_addr() -
				                (Genode::addr_t)noux_config_node.content_addr() + 1;

				if ((sizeof(config_buf) - config_bytes_written) < bytes_to_copy)
					throw Config_buffer_overflow();

				strncpy(&config_buf[config_bytes_written],
					    noux_config_node.content_addr(),
					    bytes_to_copy);

				/* subtract the byte for '\0' again */
				config_bytes_written += bytes_to_copy - 1;

				/*
				 * Create a zero-terminated string for the GDB target config node
				 */

				char target_config[target_config_size + 1];
				if (target_config_addr)
					Genode::strncpy(target_config, target_config_addr, sizeof(target_config));
				else
					target_config[0] = '\0';

				/*
				 * Insert the '<config>' node for 'gdb_monitor'
				 */

				config_bytes_written += Genode::snprintf(&config_buf[config_bytes_written],
				                        (sizeof(config_buf) - config_bytes_written), "\n \
					<config>\n \
						<target name=\"%s\">\n \
							%s \
						</target>\n \
						<preserve name=\"RAM\" quantum=\"%zu\"/>\n \
					</config> \
				",
				binary_name,
				target_config,
				(size_t)gdb_ram_preserve);

				/*
				 * Copy the third (and final) part of the config file template
				 */

				/* Genode::strncpy() makes the last character '\0', so we need to add 1 byte */
				bytes_to_copy = ((Genode::addr_t)init_config_node.addr() +
				                 init_config_node.size()) -
				                (Genode::addr_t)gdb_monitor_node.content_addr() + 1;

				if ((sizeof(config_buf) - config_bytes_written) < bytes_to_copy)
					throw Config_buffer_overflow();

				strncpy(&config_buf[config_bytes_written],
					    gdb_monitor_node.content_addr(),
					    bytes_to_copy);

				/* subtract the byte for '\0' again */
				config_bytes_written += bytes_to_copy - 1;

				return Xml_node(config_buf, config_bytes_written);

			} catch (Config_buffer_overflow) {
				tprintf(terminal, "Error: the buffer for the generated GDB "
				                  "subsystem configuration is too small.\n");
				throw Child_configuration_failed();
			} catch (Binary_elf_check_failed) {
				tprintf(terminal, "Error: could not determine link type of the "
				                  "GDB target binary.\n");
				throw Child_configuration_failed();
			}
		}

		void _execute_subsystem(char const *name, Command_line &cmd,
		                        Terminal::Session &terminal,
		                        Xml_node subsystem_node)
		{
			Genode::Number_of_bytes ram = 0;
			Genode::Number_of_bytes ram_limit = 0;
			Genode::Number_of_bytes gdb_ram_preserve = 10*1024*1024;

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

			cmd.parameter("--ram",       ram);
			cmd.parameter("--ram-limit", ram_limit);
			cmd.parameter("--gdb-ram-preserve", gdb_ram_preserve);

			/* account for cli_monitor local metadata */
			size_t preserve_ram = 100*1024;
			if ((ram + preserve_ram) > Genode::env()->ram_session()->avail()) {
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

				/* create child configuration */
				const char *target_config_addr = 0;
				size_t target_config_size = 0;
				try { 
					Xml_node target_config_node = subsystem_node.sub_node("config");
					target_config_addr = target_config_node.addr();
					target_config_size = target_config_node.size();
				} catch (...) { }

				Xml_node config_node = _gdb_config_node(binary_name,
				                                        target_config_addr,
				                                        target_config_size,
				                                        gdb_ram_preserve,
				                                        terminal);

				/* create child */
				child = new (Genode::env()->heap())
					Gdb_command_child(_ram, label, "init", _cap, ram, ram_limit,
					                  _yield_response_sigh_cap, _kill_gdb_sig_cap,
					                  terminal);

				/* configure child */
				try {
					child->configure(config_node.addr(), config_node.size());
					if (verbose)
						tprintf(terminal, "     config: inline\n");
				} catch (...) {
					if (verbose)
						tprintf(terminal, "     config: none\n");
				}

			}
			catch (Child_configuration_failed) {
				return;
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

			_children.insert(child);
			child->start();
		}

	public:

		Gdb_command(Ram &ram, Genode::Cap_session &cap, Child_registry &children,
		            Subsystem_config_registry &subsustem_configs,
		            Signal_context_capability yield_response_sigh_cap,
		            Signal_context_capability kill_gdb_sig_cap)
		:
			Command("gdb", "create new subsystem with GDB"),
			_ram(ram), _children(children), _cap(cap),
			_subsystem_configs(subsustem_configs),
			_yield_response_sigh_cap(yield_response_sigh_cap),
			_kill_gdb_sig_cap(kill_gdb_sig_cap)
		{
			add_parameter(new Parameter("--ram",       Parameter::NUMBER, "initial RAM quota"));
			add_parameter(new Parameter("--ram-limit", Parameter::NUMBER, "limit for expanding RAM quota"));
			add_parameter(new Parameter("--gdb-ram-preserve", Parameter::NUMBER,
			              "RAM quota which GDB monitor should preserve for itself (default: 5M)"));
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
			/* check if the GDB-related ROM modules are available */
			try {
				Genode::Rom_connection gdb_command_config("gdb_command_config");
				Genode::Rom_connection terminal_crosslink("terminal_crosslink");
				Genode::Rom_connection noux("noux");
				Genode::Rom_connection gdb_monitor("gdb_monitor");
			} catch (Genode::Rom_connection::Rom_connection_failed) {
				tprintf(terminal, "Error: The 'gdb' command needs the following ROM "
				                  "modules (of which some are currently missing): "
				                  "gdb_command_config, terminal_crosslink, noux, ",
				                  "gdb_monitor\n");
				return;
			}

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
};

#endif /* _GDB_COMMAND_H_ */
