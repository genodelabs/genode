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
#include <construct.h>
#include <noux_session/sysio.h>
#include <vfs_io_channel.h>
#include <terminal_io_channel.h>
#include <user_info.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <kill_broadcaster.h>
#include <vfs/dir_file_system.h>

namespace Noux {

	static Noux::Child *init_child;
	static int exit_value = ~0;

	bool init_process(Child *child) { return child == init_child; }
	void init_process_exited(int exit) { init_child = 0; exit_value = exit; }
}

extern void init_network();


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
		Genode::Xml_node arg_node = start_node.sub_node("env");
		for (; ; arg_node = arg_node.next("env")) {
			static char name_buf[256], value_buf[256];

			arg_node.attribute("name").value(name_buf, sizeof(name_buf));
			arg_node.attribute("value").value(value_buf, sizeof(value_buf));

			Genode::size_t env_var_size = Genode::strlen(name_buf) +
			                              Genode::strlen("=") +
			                              Genode::strlen(value_buf) + 1;
			if (index + env_var_size < sizeof(env)) {
				Genode::snprintf(&env[index], env_var_size, "%s=%s", name_buf, value_buf);
				index += env_var_size;
			} else {
				env[index] = 0;
				break;
			}
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
              Vfs::Dir_file_system                        &root,
              Noux::Vfs_handle_context                    &vfs_handle_context,
              Noux::Vfs_io_waiter_registry                &vfs_io_waiter_registry,
              Noux::Terminal_io_channel::Type              type,
              Genode::Allocator                           &alloc)
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
		Vfs_io_channel(path.string(), root.leaf_path(path.string()), &root,
		               vfs_handle, vfs_handle_context,
		               vfs_io_waiter_registry, env.ep());
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
	Noux::Parent_services _parent_services;

	Noux::Parent_service _log_parent_service   { _parent_services, "LOG" };
	Noux::Parent_service _timer_parent_service { _parent_services, "Timer" };

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

	struct Io_response_handler : Vfs::Io_response_handler
	{
		Vfs_io_waiter_registry io_waiter_registry;

		void handle_io_response(Vfs::Vfs_handle::Context *context) override
		{
			if (context) {
				Vfs_handle_context *vfs_handle_context = static_cast<Vfs_handle_context*>(context);
				vfs_handle_context->vfs_io_waiter.wakeup();
				return;
			}

			io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
				r.wakeup();
			});
		}

	} _io_response_handler;

	Vfs::Dir_file_system _root_dir { _env, _heap, _config.xml().sub_node("fstab"),
	                                 _io_response_handler,
	                                 _global_file_system_factory,
	                                 Vfs::Dir_file_system::Root() };

	Vfs_handle_context _vfs_handle_context;

	Pid_allocator _pid_allocator;

	Timeout_scheduler _timeout_scheduler { _env };

	User_info _user_info { _config.xml() };

	bool _network_initialized = (init_network(), true);

	Signal_handler<Main> _destruct_handler {
		_env.ep(), *this, &Main::_handle_destruct };

	Destruct_queue _destruct_queue { _destruct_handler };

	void _handle_destruct()
	{
		_destruct_queue.flush();

		/* let noux exit if the init process exited */
		if (!init_child)
			_env.parent().exit(exit_value);
	}

	struct Kill_broadcaster_impl: Kill_broadcaster
	{
		Family_member *init_process = nullptr;

		bool kill(int pid, Noux::Sysio::Signal sig)
		{
			return init_process->deliver_kill(pid, sig);
		}

	} _kill_broadcaster;

	Noux::Child _init_child { _name_of_init_process(),
	                          _verbose,
	                          _user_info,
	                          0,
	                          _kill_broadcaster,
	                          _timeout_scheduler,
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

	Constructible<Terminal::Connection> _terminal;

	/*
	 * I/O channels must be dynamically allocated to handle cases where the
	 * init program closes one of these.
	 */
	typedef Terminal_io_channel Tio; /* just a local abbreviation */

	Shared_pointer<Io_channel>
		_channel_0 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		             _vfs_handle_context, _io_response_handler.io_waiter_registry,
		             Tio::STDIN,  _heap), _heap },
		_channel_1 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		            _vfs_handle_context, _io_response_handler.io_waiter_registry,
		             Tio::STDOUT, _heap), _heap },
		_channel_2 { &connect_stdio(_env, _terminal, _config.xml(), _root_dir,
		             _vfs_handle_context, _io_response_handler.io_waiter_registry,
		             Tio::STDERR, _heap), _heap };

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


void noux_construct(Genode::Env &env)
{
	static Noux::Main main(env);
}
