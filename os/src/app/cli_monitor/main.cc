/*
 * \brief  Simple command-line interface for managing Genode subsystems
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/child.h>
#include <init/child_policy.h>
#include <os/config.h>
#include <os/child_policy_dynamic_rom.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <rm_session/connection.h>
#include <cap_session/connection.h>
#include <rom_session/connection.h>

/* local includes */
#include <line_editor.h>
#include <command_line.h>
#include <terminal_util.h>
#include <extension.h>

using Terminal::tprintf;
using Genode::Xml_node;


/***************
 ** Utilities **
 ***************/

static inline void tprint_bytes(Terminal::Session &terminal, size_t bytes)
{
	enum { KB = 1024, MB = 1024*KB };
	if (bytes > MB) {
		size_t const mb = bytes / MB;

		tprintf(terminal, "%zd.", mb);
		size_t const remainder = bytes - (mb * MB);

		tprintf(terminal, "%zd MiB", (remainder*100)/(MB));
		return;
	}

	if (bytes > KB) {
		size_t const kb = bytes / KB;

		tprintf(terminal, "%zd.", kb);
		size_t const remainder = bytes - (kb * KB);

		tprintf(terminal, "%zd KiB", (remainder*100)/(KB));
		return;
	}

	tprintf(terminal, "%zd bytes", bytes);
}


static inline void tprint_status_bytes(Terminal::Session &terminal,
                                       char const *label, size_t bytes)
{
	tprintf(terminal, label);
	tprint_bytes(terminal, bytes);
	tprintf(terminal, "\n");
}


inline void *operator new (size_t size)
{
	return Genode::env()->heap()->alloc(size);
}


/********************
 ** Child handling **
 ********************/

class Child : public List<Child>::Element, Genode::Child_policy
{
	public:

		/*
		 * XXX derive donated quota from information to be provided by
		 *     the used 'Connection' interfaces
		 */
		enum { DONATED_RAM_QUOTA = 128*1024 };

		class Quota_exceeded : public Genode::Exception { };

	private:

		struct Label
		{
			enum { LABEL_MAX_LEN = 128 };
			char buf[LABEL_MAX_LEN];
			Label(char const *label) { strncpy(buf, label, sizeof(buf)); }
		};

		Label const _label;

		Argument _kill_argument;

		struct Resources
		{
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			Resources(const char *label, Genode::size_t ram_quota)
			: ram(label), cpu(label)
			{
				if (ram_quota >  DONATED_RAM_QUOTA)
					ram_quota -= DONATED_RAM_QUOTA;
				else
					throw Quota_exceeded();
				ram.ref_account(Genode::env()->ram_session_cap());
				if (Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota) != 0)
					throw Quota_exceeded();
			}
		};

		Resources                             _resources;
		Genode::Service_registry              _parent_services;
		Genode::Rom_connection                _binary_rom;

		enum { ENTRYPOINT_STACK_SIZE = 12*1024 };
		Genode::Rpc_entrypoint _entrypoint;

		Init::Child_policy_enforce_labeling   _labeling_policy;
		Init::Child_policy_provide_rom_file   _binary_policy;
		Genode::Child_policy_dynamic_rom_file _config_policy;
		Genode::Child                         _child;

	public:

		Child(char          const *label,
		      char          const *binary,
		      Genode::Cap_session &cap_session,
		      Genode::size_t       ram_quota)
		:
			_label(label),
			_kill_argument(label, "subsystem"),
			_resources(_label.buf, ram_quota),
			_binary_rom(binary, _label.buf),
			_entrypoint(&cap_session, ENTRYPOINT_STACK_SIZE, _label.buf, false),
			_labeling_policy(_label.buf),
			_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
			_config_policy("config", _entrypoint, &_resources.ram),
			_child(_binary_rom.dataspace(),
			       _resources.ram.cap(), _resources.cpu.cap(),
			       _resources.rm.cap(), &_entrypoint, this)
		{ }

		void configure(char const *config, size_t config_len)
		{
			_config_policy.load(config, config_len);
		}

		void start()
		{
			_entrypoint.activate();
		}

		Argument *kill_argument() { return &_kill_argument; }


		/****************************
		 ** Child_policy interface **
		 ****************************/

		const char *name() const { return _label.buf; }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			Genode::Service *service = 0;

			/* check for binary file request */
			if ((service = _binary_policy.resolve_session_request(service_name, args)))
				return service;

			/* check for config file request */
			if ((service = _config_policy.resolve_session_request(service_name, args)))
				return service;

			/* fill parent service registry on demand */
			if (!(service = _parent_services.find(service_name))) {
				service = new (Genode::env()->heap())
				          Genode::Parent_service(service_name);
				_parent_services.insert(service);
			}

			/* return parent service */
			return service;
		}

		void filter_session_args(const char *service,
		                         char *args, Genode::size_t args_len)
		{
			_labeling_policy.filter_session_args(service, args, args_len);
		}
};


class Child_registry : public List<Child>
{
	private:

		/**
		 * Return true if a child with the specified name already exists
		 */
		bool _child_name_exists(const char *label)
		{
			for (Child *child = first() ; child; child = child->next())
				if (strcmp(child->name(), label) == 0)
					return true;
			return false;
		}

	public:

		enum { CHILD_NAME_MAX_LEN = 64 };

		/**
		 * Produce new unique child name
		 */
		void unique_child_name(const char *prefix, char *dst, int dst_len)
		{
			char buf[CHILD_NAME_MAX_LEN];
			char suffix[8];
			suffix[0] = 0;

			for (int cnt = 1; true; cnt++) {

				/* build program name composed of prefix and numeric suffix */
				snprintf(buf, sizeof(buf), "%s%s", prefix, suffix);

				/* if such a program name does not exist yet, we are happy */
				if (!_child_name_exists(buf)) {
					strncpy(dst, buf, dst_len);
					return;
				}

				/* increase number of suffix */
				snprintf(suffix, sizeof(suffix), ".%d", cnt + 1);
			}
		}
};


/**************
 ** Commands **
 **************/

struct Help_command : Command
{
	Help_command() : Command("help", "brief help information") { }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		tprintf(terminal, "  Press [tab] for a list of commands.\n");
		tprintf(terminal, "  When given a command, press [tab] for a list of arguments.\n");
	}
};


struct Kill_command : Command
{
	Child_registry &_children;

	void _destroy_child(Child *child, Terminal::Session &terminal)
	{
		tprintf(terminal, "destroying subsystem '%s'\n", child->name());
		remove_argument(child->kill_argument());
		_children.remove(child);
		Genode::destroy(Genode::env()->heap(), child);
	}

	Kill_command(Child_registry &children)
	:
		Command("kill", "destroy subsystem"),
		_children(children)
	{
		add_parameter(new Parameter("--all", Parameter::VOID, "kill all subsystems"));
	}

	void execute(Command_line &cmd, Terminal::Session &terminal)
	{
		bool const kill_all = cmd.parameter_exists("--all");

		if (kill_all) {
			for (Child *child = _children.first(); child; child = _children.first())
				_destroy_child(child, terminal);
			return;
		}

		char label[128];
		label[0] = 0;
		if (cmd.argument(0, label, sizeof(label)) == false) {
			tprintf(terminal, "Error: no configuration name specified\n");
			return;
		}

		/* lookup child by its unique name */
		for (Child *child = _children.first(); child; child = child->next()) {
			if (strcmp(child->name(), label) == 0) {
				_destroy_child(child, terminal);
				return;
			}
		}

		tprintf(terminal, "Error: subsystem '%s' does not exist\n", label);
	}
};


struct Start_command : Command
{
	Child_registry      &_children;
	Genode::Cap_session &_cap;
	Xml_node             _config;
	Kill_command        &_kill_command;

	Start_command(Genode::Cap_session &cap, Child_registry &children,
	              Xml_node config, Kill_command &kill_command)
	:
		Command("start", "create new subsystem"),
		_children(children), _cap(cap), _config(config),
		_kill_command(kill_command)
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

				add_argument(new Argument(name, help));
			}
		} catch (Xml_node::Nonexistent_sub_node) { /* end of list */ }

		add_parameter(new Parameter("--count",   Parameter::NUMBER, "number of instances"));
		add_parameter(new Parameter("--ram",     Parameter::NUMBER, "RAM quota"));
		add_parameter(new Parameter("--verbose", Parameter::VOID,   "show diagnostics"));
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
					break;
				}
			}
		} catch (...) { }

		cmd.parameter("--count", count);
		cmd.parameter("--ram",   ram);

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
				tprintf(terminal, "     binary: %s\n", binary_name);
			}

			Child *child = 0;
			try {
				child = new (Genode::env()->heap())
					Child(label, binary_name, _cap, ram);
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

			_kill_command.add_argument(child->kill_argument());
			_children.insert(child);
			child->start();
		}
	}
};


struct Status_command : Command
{
	Status_command() : Command("status", "show runtime status") { }

	void execute(Command_line &, Terminal::Session &terminal)
	{
		Genode::Ram_session *ram = Genode::env()->ram_session();

		tprint_status_bytes(terminal, "  RAM quota: ", ram->quota());
		tprint_status_bytes(terminal, "       used: ", ram->used());
		tprint_status_bytes(terminal, "      avail: ", ram->avail());
	}
};


/******************
 ** Main program **
 ******************/

static inline Command *lookup_command(char const *buf, Command_registry &registry)
{
	Token token(buf);
	for (Command *curr = registry.first(); curr; curr = curr->next())
		if (strcmp(token.start(), curr->name(), token.len()) == 0
		 && strlen(curr->name()) == token.len())
			return curr;
	return 0;
}


int main(int argc, char **argv)
{
	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	using Genode::Signal_context;
	using Genode::Signal_receiver;

	try { Genode::config()->xml_node(); }
	catch (...) {
		PERR("Error: could not obtain configuration");
		return -1;
	}

	static Genode::Cap_connection cap;
	static Terminal::Connection   terminal;
	static Command_registry       commands;
	static Child_registry         children;

	/* initialize platform-specific commands */
	init_extension(commands);

	/* initialize generic commands */
	commands.insert(new Help_command);
	Kill_command kill_command(children);
	commands.insert(&kill_command);
	commands.insert(new Start_command(cap, children,
	                                  Genode::config()->xml_node(),
	                                  kill_command));
	commands.insert(new Status_command);

	static Signal_receiver sig_rec;
	static Signal_context  read_avail_sig_ctx;
	terminal.read_avail_sigh(sig_rec.manage(&read_avail_sig_ctx));

	for (;;) {

		enum { COMMAND_MAX_LEN = 1000 };
		static char buf[COMMAND_MAX_LEN];

		Line_editor line_editor("genode> ", buf, sizeof(buf), terminal, commands);

		while (!line_editor.is_complete()) {

			/* block for event, e.g., the arrival of new user input */
			sig_rec.wait_for_signal();

			/* supply pending terminal input to line editor */
			while (terminal.avail() && !line_editor.is_complete()) {
				char c;
				terminal.read(&c, 1);
				line_editor.submit_input(c);
			}
		}

		Command *command = lookup_command(buf, commands);
		if (!command) {
			Token cmd_name(buf);
			tprintf(terminal, "Error: unknown command \"");
			terminal.write(cmd_name.start(), cmd_name.len());
			tprintf(terminal, "\"\n");
			continue;
		}

		/* validate parameters against command meta data */
		Command_line cmd_line(buf, *command);
		Token unexpected = cmd_line.unexpected_parameter();
		if (unexpected) {
			tprintf(terminal, "Error: unexpected parameter \"");
			terminal.write(unexpected.start(), unexpected.len());
			tprintf(terminal, "\"\n");
			continue;
		}
		command->execute(cmd_line, terminal);
	}

	return 0;
}
