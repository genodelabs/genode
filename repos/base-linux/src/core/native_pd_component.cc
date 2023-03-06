/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <cpu/consts.h>

/* core includes */
#include <pd_session_component.h>
#include <dataspace_component.h>

/* base-internal includes */
#include <base/internal/parent_socket_handle.h>
#include <base/internal/capability_space_tpl.h>

/* Linux includes */
#include <core_linux_syscalls.h>

using namespace Core;


/***************
 ** Utilities **
 ***************/

/**
 * Argument frame and initial stack for passing 'execve' parameters through 'clone'
 */
struct Execve_args_and_stack
{
	struct Args
	{
		char const *filename;
		char const **argv;
		char const **envp;
		Lx_sd       parent_sd;
	};

	Args args;

	/* initial stack used by the child until calling 'execve' */
	enum { STACK_SIZE = 4096 };
	char stack[STACK_SIZE];

	void *initial_sp()
	{
		return (void *)Abi::stack_align((addr_t)(stack + STACK_SIZE));
	}
};


static Execve_args_and_stack &_execve_args_and_stack()
{
	static Execve_args_and_stack inst { };
	return inst;
}


/**
 * Startup code of the new child process
 */
static int _exec_child()
{
	auto const &args = _execve_args_and_stack().args;

	lx_dup2(args.parent_sd.value, PARENT_SOCKET_HANDLE);

	return lx_execve(args.filename, args.argv, args.envp);
}


/**
 * List of Unix environment variables, initialized by the startup code
 */
extern char **lx_environ;


/**
 * Read environment variable as string
 *
 * If no matching key exists, return an empty string.
 */
static const char *get_env(const char *key)
{
	Genode::size_t key_len = Genode::strlen(key);
	for (char **curr = lx_environ; curr && *curr; curr++)
		if ((Genode::strcmp(*curr, key, key_len) == 0) && (*curr)[key_len] == '=')
			return (const char *)(*curr + key_len + 1);

	return "";
}


/**************************
 ** PD session interface **
 **************************/

void Native_pd_component::_start(Dataspace_component &ds)
{
	const char *tmp_filename = "temporary_executable_elf_dataspace_file_for_execve";

	/* we need 's' on stack to make it an lvalue with an lvalue member we use the pointer to */
	Linux_dataspace::Filename filename = ds.fname();

	/*
	 * In order to be executable via 'execve', a program must be represented as
	 * a file on the Linux file system. However, this is not the case for a
	 * plain RAM dataspace that contains an ELF image. In this case, we copy
	 * the dataspace content into a temporary file whose path is passed to
	 * 'execve()'.
	 */
	if (filename == "") {

		filename = tmp_filename;

		int tmp_binary_fd = lx_open(filename.string(), O_CREAT | O_EXCL | O_WRONLY, S_IRWXU);
		if (tmp_binary_fd < 0) {
			error("Could not create file '", filename, "'");
			return; /* XXX reflect error to client */
		}

		char buf[4096];
		int num_bytes = 0;
		int const fd_socket = Capability_space::ipc_cap_data(ds.fd()).dst.socket.value;
		while ((num_bytes = lx_read(fd_socket, buf, sizeof(buf))) != 0)
			lx_write(tmp_binary_fd, buf, num_bytes);

		lx_close(tmp_binary_fd);
	}

	/* pass parent capability as environment variable to the child */
	using Env_string = String<256>;
	static Env_string env_strings[] {
		{ "parent_local_name=", _pd_session._parent.local_name() },
		{ "DISPLAY=",           get_env("DISPLAY") },
		{ "HOME=",              get_env("HOME") },
		{ "LD_LIBRARY_PATH=",   get_env("LD_LIBRARY_PATH") },
	};
	char const *env[] = { env_strings[0].string(), env_strings[1].string(),
	                      env_strings[2].string(), env_strings[3].string(),
	                      nullptr };

	/* prefix name of Linux program (helps killing some zombies) */
	using Pname = String<Session::Label::capacity() + 9>;
	Pname const pname("[Genode] ", _pd_session._label);
	char const *argv_buf[] { pname.string(), nullptr };

	_execve_args_and_stack().args = Execve_args_and_stack::Args {
		.filename  = filename.string(),
		.argv      = argv_buf,
		.envp      = env,
		.parent_sd = Capability_space::ipc_cap_data(_pd_session._parent).dst.socket
	};

	_pid = lx_create_process((int (*)())_exec_child,
	                         _execve_args_and_stack().initial_sp());

	if (filename == tmp_filename)
		lx_unlink(filename.string());
}


Native_pd_component::Native_pd_component(Pd_session_component &pd, const char *)
:
	_pd_session(pd)
{
	_pd_session._ep.manage(this);
}


Native_pd_component::~Native_pd_component()
{
	if (_pid)
		lx_kill((int)_pid, 9);

	_pd_session._ep.dissolve(this);
}


void Native_pd_component::start(Capability<Dataspace> binary)
{
	/* lookup binary dataspace */
	_pd_session._ep.apply(binary, [&] (Dataspace_component *ds) {

		if (ds)
			_start(*ds);
		else
			error("failed to lookup binary to start");
	});
};
