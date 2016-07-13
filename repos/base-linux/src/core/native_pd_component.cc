/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/log.h>
#include <base/snprintf.h>

/* core-local includes */
#include <pd_session_component.h>
#include <dataspace_component.h>

/* base-internal includes */
#include <base/internal/parent_socket_handle.h>
#include <base/internal/capability_space_tpl.h>

/* Linux includes */
#include <core_linux_syscalls.h>

using namespace Genode;


/***************
 ** Utilities **
 ***************/

/**
 * Argument frame for passing 'execve' paremeters through 'clone'
 */
struct Execve_args
{
	char         const *filename;
	char       * const *argv;
	char       * const *envp;
	int          const parent_sd;

	Execve_args(char   const *filename,
	            char * const *argv,
	            char * const *envp,
	            int           parent_sd)
	:
		filename(filename), argv(argv), envp(envp), parent_sd(parent_sd)
	{ }
};


/**
 * Startup code of the new child process
 */
static int _exec_child(Execve_args *arg)
{
	lx_dup2(arg->parent_sd, PARENT_SOCKET_HANDLE);

	return lx_execve(arg->filename, arg->argv, arg->envp);
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
	Linux_dataspace::Filename s = ds.fname();
	const char *filename = s.buf;

	/*
	 * In order to be executable via 'execve', a program must be represented as
	 * a file on the Linux file system. However, this is not the case for a
	 * plain RAM dataspace that contains an ELF image. In this case, we copy
	 * the dataspace content into a temporary file whose path is passed to
	 * 'execve()'.
	 */
	if (strcmp(filename, "") == 0) {

		filename = tmp_filename;

		int tmp_binary_fd = lx_open(filename, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU);
		if (tmp_binary_fd < 0) {
			error("Could not create file '", filename, "'");
			return; /* XXX reflect error to client */
		}

		char buf[4096];
		int num_bytes = 0;
		int const fd_socket = Capability_space::ipc_cap_data(ds.fd()).dst.socket;
		while ((num_bytes = lx_read(fd_socket, buf, sizeof(buf))) != 0)
			lx_write(tmp_binary_fd, buf, num_bytes);

		lx_close(tmp_binary_fd);
	}

	/* pass parent capability as environment variable to the child */
	enum { ENV_STR_LEN = 256 };
	static char envbuf[5][ENV_STR_LEN];
	Genode::snprintf(envbuf[1], ENV_STR_LEN, "parent_local_name=%lu",
	                 _pd_session._parent.local_name());
	Genode::snprintf(envbuf[2], ENV_STR_LEN, "DISPLAY=%s",
	                 get_env("DISPLAY"));
	Genode::snprintf(envbuf[3], ENV_STR_LEN, "HOME=%s",
	                 get_env("HOME"));
	Genode::snprintf(envbuf[4], ENV_STR_LEN, "LD_LIBRARY_PATH=%s",
	                 get_env("LD_LIBRARY_PATH"));

	char *env[] = { &envbuf[0][0], &envbuf[1][0], &envbuf[2][0],
		&envbuf[3][0], &envbuf[4][0], 0 };

	/* prefix name of Linux program (helps killing some zombies) */
	char const *prefix = "[Genode] ";
	char pname_buf[sizeof(_pd_session._label) + sizeof(prefix)];
	snprintf(pname_buf, sizeof(pname_buf), "%s%s", prefix, _pd_session._label.string);
	char *argv_buf[2];
	argv_buf[0] = pname_buf;
	argv_buf[1] = 0;

	/*
	 * We cannot create the new process via 'fork()' because all our used
	 * memory including stack memory is backed by dataspaces, which had been
	 * mapped with the 'MAP_SHARED' flag. Therefore, after being created, the
	 * new process starts using the stack with the same physical memory pages
	 * as used by parent process. This would ultimately lead to stack
	 * corruption. To prevent both processes from concurrently accessing the
	 * same stack, we pause the execution of the parent until the child calls
	 * 'execve'. From then on, the child has its private memory layout. The
	 * desired behaviour is normally provided by 'vfork' but we use the more
	 * modern 'clone' call for this purpose.
	 */
	enum { STACK_SIZE = 4096 };
	static char stack[STACK_SIZE];    /* initial stack used by the child until
	                                     calling 'execve' */

	/*
	 * Argument frame as passed to 'clone'. Because, we can only pass a single
	 * pointer, all arguments are embedded within the 'execve_args' struct.
	 */
	Execve_args arg(filename, argv_buf, env,
	                Capability_space::ipc_cap_data(_pd_session._parent).dst.socket);

	_pid = lx_create_process((int (*)(void *))_exec_child,
	                         stack + STACK_SIZE - sizeof(umword_t), &arg);

	if (strcmp(filename, tmp_filename) == 0)
		lx_unlink(filename);
}


Native_pd_component::Native_pd_component(Pd_session_component &pd_session,
                                         const char *args)
:
	_pd_session(pd_session)
{
	_pd_session._thread_ep.manage(this);
}


Native_pd_component::~Native_pd_component()
{
	if (_pid)
		lx_kill(_pid, 9);

	_pd_session._thread_ep.dissolve(this);
}


void Native_pd_component::start(Capability<Dataspace> binary)
{
	/* lookup binary dataspace */
	_pd_session._thread_ep.apply(binary, [&] (Dataspace_component *ds) {

		if (ds)
			_start(*ds);
		else
			error("failed to lookup binary to start");
	});
};
