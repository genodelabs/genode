/*
 * \brief  Unix emulation environment for Genode
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/component.h>
#include <libc/component.h>

/* Noux includes */
#include <child.h>
#include <noux_session/sysio.h>
#include <vfs_io_channel.h>
#include <terminal_io_channel.h>
#include <user_info.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <kill_broadcaster.h>
#include <vfs/dir_file_system.h>
#include <vfs/simple_env.h>
#include <time_info.h>

namespace Noux {

	static Noux::Child *init_child;
	static int exit_value = ~0;

	bool init_process(Child *child) { return child == init_child; }
	void init_process_exited(int exit) { init_child = 0; exit_value = exit; }
}


Noux::Io_receptor_registry * Noux::io_receptor_registry()
{
	static Noux::Io_receptor_registry _inst;
	return &_inst;
}


/**
 * Return string containing the environment variables of init
 *
 * The variable definitions are separated by zeros. The end of the string is
 * marked with another zero.
 */
static Noux::Sysio::Env &env_string_of_init_process(Genode::Xml_node config)
{
	static Noux::Sysio::Env env;
	int index = 0;

	/* read environment variables for init process from config */
	Genode::Xml_node start_node = config.sub_node("start");
	try {
		Genode::Xml_node node = start_node.sub_node("env");
		for (; ; node = node.next("env")) {

			typedef Genode::String<256> Var;
			Var const var(node.attribute_value("name", Var()), "=",
			              node.attribute_value("value", Var()));

			bool const env_exeeded = index + var.length() >= sizeof(env);
			bool const var_exeeded = (var.length() == var.capacity());

			if (env_exeeded || var_exeeded) {
				warning("truncated environment variable: ", node);
				env[index] = 0;
				break;
			}

			Genode::strncpy(&env[index], var.string(), var.length());
			index += var.length();
		}
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }

	return env;
}


namespace Noux { class Stdio_unavailable : Genode::Exception { }; }


/*
 * \throw Stdio_unavailable
 */
static Noux::Io_channel &
connect_stdio(Genode::Env                                 &env,
              Genode::Constructible<Terminal::Connection> &terminal,
              Genode::Xml_node                             config,
              Vfs::File_system                            &root,
              Noux::Vfs_io_waiter_registry                &vfs_io_waiter_registry,
              Noux::Terminal_io_channel::Type              type,
              Genode::Allocator                           &alloc,
              Noux::Time_info                             &time_info,
              Timer::Connection                           &timer)
{
	using namespace Vfs;
	using namespace Noux;
	typedef Terminal_io_channel Tio; /* just a local abbreviation */

	typedef Genode::String<MAX_PATH_LEN> Path;
	Vfs_handle *vfs_handle = nullptr;
	char const *stdio_name = "";
	unsigned mode = 0;

	switch (type) {
	case Tio::STDIN:
		stdio_name = "stdin";
		mode = Directory_service::OPEN_MODE_RDONLY;
		break;
	case Tio::STDOUT:
		stdio_name = "stdout";
		mode = Directory_service::OPEN_MODE_WRONLY;
		break;
	case Tio::STDERR:
		stdio_name = "stderr";
		mode = Directory_service::OPEN_MODE_WRONLY;
		break;
	};

	if (!config.has_attribute(stdio_name)) {
		if (!terminal.constructed())
			terminal.construct(env);
		warning(stdio_name, " VFS path not defined, connecting to terminal session");
		return *new (alloc) Tio(*terminal, type, env.ep());
	}

	Path const path = config.attribute_value(stdio_name, Path());

	if (root.open(path.string(), mode, &vfs_handle, alloc)
	    != Directory_service::OPEN_OK)
	{
		error("failed to connect ", stdio_name, " to '", path, "'");
		throw Stdio_unavailable();
	}

	return *new (alloc)
		Vfs_io_channel(path.string(), root.leaf_path(path.string()),
		               vfs_handle, vfs_io_waiter_registry, env.ep(),
		               time_info, timer);
}


/*
 * This lock is needed to delay the insertion of signals into a child object.
 * This is necessary during an 'execve()' syscall, when signals get copied from
 * the old child object to the new one. Without the lock, an IO channel could
 * insert a signal into both objects, which could lead to a duplicated signal
 * in the new child object.
 */
Genode::Lock &Noux::signal_lock()
{
	static Genode::Lock inst;
	return inst;
}


namespace Noux { struct Main; }


struct Noux::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	/* whitelist of service requests to be routed to the parent */
	Noux::Parent_services _parent_services { };

	Noux::Parent_service _log_parent_service   { _parent_services, _env, "LOG" };
	Noux::Parent_service _timer_parent_service { _parent_services, _env, "Timer" };

	Attached_rom_dataspace _config { _env, "config" };

	Verbose _verbose { _config.xml() };

	/**
	 * Return name of init process as specified in the config
	 */
	Child_policy::Name _name_of_init_process() const
	{
		return _config.xml().sub_node("start").attribute_value("name", Child_policy::Name());
	}

	/**
	 * Read command-line arguments of init process from config
	 */
	Args const &_args_of_init_process()
	{
		static char args_buf[4096];
		static Args args(args_buf, sizeof(args_buf));

		Xml_node start_node = _config.xml().sub_node("start");
		try {
			/* the first argument is the program name */
			args.append(_name_of_init_process().string());

			start_node.for_each_sub_node("arg", [&] (Xml_node arg_node) {
				typedef String<512> Value;
				args.append(arg_node.attribute_value("value", Value()).string());
			});

		} catch (Args::Overrun) { error("argument buffer overrun"); }

		return args;
	}

	/* initialize virtual file system */
	Vfs::Global_file_system_factory _global_file_system_factory { _heap };

	struct Io_progress_handler : Genode::Entrypoint::Io_progress_handler
	{
		Vfs_io_waiter_registry io_waiter_registry { };

		Io_progress_handler(Genode::Entrypoint &ep)
		{
			ep.register_io_progress_handler(*this);
		}

		void handle_io_progress() override
		{
			io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
				r.wakeup();
			});
		}

	} _io_response_handler { _env.ep() };

	Vfs::Simple_env _vfs_env { _env, _heap, _config.xml().sub_node("fstab") };

	Vfs::File_system &_root_dir = _vfs_env.root_dir();

	Pid_allocator _pid_allocator { };

	Timer::Connection _timer_connection { _env };

	User_info _user_info { _config.xml() };

	Time_info _time_info { _env, _config.xml() };

	Signal_handler<Main> _destruct_handler {
		_env.ep(), *this, &Main::_handle_destruct };

	Destruct_queue _destruct_queue { _destruct_handler };

	void _handle_destruct()
	{
		_destruct_queue.flush();

		/* let noux exit if the init process exited */
		if (!init_child) {
			_channel_0 = Shared_pointer<Io_channel>();
			_channel_1 = Shared_pointer<Io_channel>();
			_channel_2 = Shared_pointer<Io_channel>();
			_env.parent().exit(exit_value);
		}
	}

	struct Kill_broadcaster_impl: Kill_broadcaster
	{
		Family_member *init_process = nullptr;

		bool kill(int pid, Noux::Sysio::Signal sig) override
		{
			return init_process->deliver_kill(pid, sig);
		}

	} _kill_broadcaster { };

	Noux::Child _init_child { _name_of_init_process(),
	                          _verbose,
	                          _user_info,
	                          _time_info,
	                          0,
	                          _kill_broadcaster,
	                          _timer_connection,
	                          _init_child,
	                          _pid_allocator,
	                          _pid_allocator.alloc(),
	                          _env,
	                          _root_dir,
	                          _io_response_handler.io_waiter_registry,
	                          _args_of_init_process(),
	                          env_string_of_init_process(_config.xml()),
	                          _heap,
	                          _env.pd(),
	                          _env.pd_session_cap(),
	                          _parent_services,
	                          false,
	                          _destruct_queue };

	Constructible<Terminal::Connection> _terminal { };

	/*
	 * I/O channels must be dynamically allocated to handle cases where the
	 * init program closes one of these.
	 */
	typedef Terminal_io_channel Tio; /* just a local abbreviation */

	Shared_pointer<Io_channel>
		_channel_0 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		             _io_response_handler.io_waiter_registry,
		             Tio::STDIN,  _heap, _time_info, _timer_connection), _heap },
		_channel_1 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		            _io_response_handler.io_waiter_registry,
		             Tio::STDOUT, _heap, _time_info, _timer_connection), _heap },
		_channel_2 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		             _io_response_handler.io_waiter_registry,
		             Tio::STDERR, _heap, _time_info, _timer_connection), _heap };

	Main(Env &env) : _env(env)
	{
		log("--- noux started ---");

		_init_child.add_io_channel(_channel_0, 0);
		_init_child.add_io_channel(_channel_1, 1);
		_init_child.add_io_channel(_channel_2, 2);

		_kill_broadcaster.init_process = &_init_child;

		init_child = &_init_child;

		_init_child.start();
	}
};


void Component::construct(Genode::Env &env)
{
	static Noux::Main main(env);
}
