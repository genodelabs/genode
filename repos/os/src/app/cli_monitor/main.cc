/*
 * \brief  Simple command-line interface for managing Genode subsystems
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <base/component.h>

/* public CLI-monitor includes */
#include <cli_monitor/ram.h>

/* local includes */
#include <line_editor.h>
#include <command_line.h>
#include <format_util.h>
#include <status_command.h>
#include <kill_command.h>
#include <start_command.h>
#include <help_command.h>
#include <yield_command.h>
#include <ram_command.h>

namespace Cli_monitor {

	struct Main;
	using namespace Genode;
}


/******************
 ** Main program **
 ******************/

struct Cli_monitor::Main
{
	Genode::Env &_env;

	Terminal::Connection _terminal { _env };

	Command_registry _commands;

	Child_registry _children;

	Command *_lookup_command(char const *buf)
	{
		Token token(buf);
		for (Command *curr = _commands.first(); curr; curr = curr->next())
			if (strcmp(token.start(), curr->name().string(), token.len()) == 0
			 && strlen(curr->name().string()) == token.len())
				return curr;
		return 0;
	}

	enum { COMMAND_MAX_LEN = 1000 };
	char _command_buf[COMMAND_MAX_LEN];
	Line_editor _line_editor {
		"genode> ", _command_buf, sizeof(_command_buf), _terminal, _commands };

	void _handle_terminal_read_avail();

	Signal_handler<Main> _terminal_read_avail_handler {
		_env.ep(), *this, &Main::_handle_terminal_read_avail };

	/**
	 * Handler for child yield responses, or RAM resource-avail signals
	 */
	void _handle_yield_response()
	{
		for (Child *child = _children.first(); child; child = child->next())
			child->try_response_to_resource_request();
	}

	Signal_handler<Main> _yield_response_handler {
		_env.ep(), *this, &Main::_handle_yield_response };

	void _handle_child_exit()
	{
		Child *next = nullptr;
		for (Child *child = _children.first(); child; child = next) {
			next = child->next();
			if (child->exited()) {
				_children.remove(child);
				Genode::destroy(_heap, child);
			}
		}
	}

	Signal_handler<Main> _child_exit_handler {
		_env.ep(), *this, &Main::_handle_child_exit };

	void _handle_yield_broadcast()
	{
		/*
		 * Compute argument of yield request to be broadcasted to all
		 * processes.
		 */
		size_t amount = 0;

		/* amount needed to reach preservation limit */
		Ram::Status ram_status = _ram.status();
		if (ram_status.avail < ram_status.preserve)
			amount += ram_status.preserve - ram_status.avail;

		/* sum of pending resource requests */
		for (Child *child = _children.first(); child; child = child->next())
			amount += child->requested_ram_quota();

		for (Child *child = _children.first(); child; child = child->next())
			child->yield(amount, true);
	}

	Signal_handler<Main> _yield_broadcast_handler {
		_env.ep(), *this, &Main::_handle_yield_broadcast };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Xml_node _vfs_config() const
	{
		try { return _config.xml().sub_node("vfs"); }
		catch (Genode::Xml_node::Nonexistent_sub_node) {
			Genode::error("missing '<vfs>' configuration");
			throw;
		}
	}

	size_t _ram_preservation_from_config() const
	{
		if (!_config.xml().has_sub_node("preservation"))
			return 0;

		return _config.xml().sub_node("preservation")
		                    .attribute_value("name", Genode::Number_of_bytes(0));
	}

	Ram _ram { _env.ram(), _env.ram_session_cap(), _ram_preservation_from_config(),
	           _yield_broadcast_handler, _yield_response_handler };

	Heap _heap { _env.ram(), _env.rm() };

	struct Io_response_handler : Vfs::Io_response_handler
	{
		void handle_io_response(Vfs::Vfs_handle::Context *) override { }
	} io_response_handler;

	Vfs::Global_file_system_factory _global_file_system_factory { _heap };

	/* initialize virtual file system */
	Vfs::Dir_file_system _root_dir { _env, _heap, _vfs_config(), io_response_handler,
	                                 _global_file_system_factory };

	Subsystem_config_registry _subsystem_config_registry { _root_dir, _heap };

	template <typename T>
	struct Registered : T
	{
		template <typename... ARGS>
		Registered(Command_registry &commands, ARGS &&... args)
		: T(args...) { commands.insert(this); }
	};

	/* initialize generic commands */
	Registered<Help_command>  _help_command    { _commands };
	Registered<Kill_command>  _kill_command    { _commands, _children, _heap };
	Registered<Start_command> _start_command   { _commands, _ram, _heap, _env.pd(),
	                                             _env.ram(), _env.ram_session_cap(),
	                                             _env.rm(), _children,
	                                             _subsystem_config_registry,
	                                             _yield_response_handler,
	                                             _child_exit_handler };
	Registered<Status_command> _status_command { _commands, _ram, _children };
	Registered<Yield_command>  _yield_command  { _commands, _children };
	Registered<Ram_command>    _ram_command    { _commands, _children, _ram };

	Main(Env &env) : _env(env)
	{
		_terminal.read_avail_sigh(_terminal_read_avail_handler);
	}
};


void Cli_monitor::Main::_handle_terminal_read_avail()
{
	/* supply pending terminal input to line editor */
	while (_terminal.avail() && !_line_editor.completed()) {
		char c = 0;
		_terminal.read(&c, 1);
		_line_editor.submit_input(c);
	}

	if (!_line_editor.completed())
		return;

	Command *command = _lookup_command(_command_buf);
	if (!command) {
		Token cmd_name(_command_buf);
		tprintf(_terminal, "Error: unknown command \"");
		_terminal.write(cmd_name.start(), cmd_name.len());
		tprintf(_terminal, "\"\n");
		_line_editor.reset();
		return;
	}

	/* validate parameters against command meta data */
	Command_line cmd_line(_command_buf, *command);
	Token unexpected = cmd_line.unexpected_parameter();
	if (unexpected) {
		tprintf(_terminal, "Error: unexpected parameter \"");
		_terminal.write(unexpected.start(), unexpected.len());
		tprintf(_terminal, "\"\n");
		_line_editor.reset();
		return;
	}
	command->execute(cmd_line, _terminal);

	/*
	 * The command might result in a change of the RAM usage. Validate
	 * that the preservation is satisfied.
	 */
	_ram.validate_preservation();
	_line_editor.reset();
}


void Component::construct(Genode::Env &env) { static Cli_monitor::Main main(env); }
