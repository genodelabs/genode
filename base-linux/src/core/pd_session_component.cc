/*
 * \brief  Core implementation of the PD session interface
 * \author Norman Feske
 * \date   2012-08-15
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/printf.h>
#include <base/snprintf.h>

/* core-local includes */
#include <pd_session_component.h>
#include <dataspace_component.h>

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
	const char  *filename;
	char *const *argv;
	char *const *envp;
	int          parent_sd;
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

Pd_session_component::Pd_session_component(Rpc_entrypoint *ep, const char *args)
:
	_pid(0), _ds_ep(ep)
{
	Arg_string::find_arg(args, "label").string(_label, sizeof(_label),
	                                           "<unlabeled>");
}


Pd_session_component::~Pd_session_component()
{
	if (_pid)
		lx_kill(_pid, 9);
}


int Pd_session_component::bind_thread(Thread_capability) { return -1;
}


int Pd_session_component::assign_parent(Parent_capability parent)
{
	_parent = parent;
	return 0;
}


void Pd_session_component::start(Capability<Dataspace> binary)
{
	/* lookup binary dataspace */
	Dataspace_component *ds =
		reinterpret_cast<Dataspace_component *>(_ds_ep->obj_by_cap(binary));

	if (!ds) {
		PERR("could not lookup binary, aborted PD startup");
		return; /* XXX reflect error to client */
	}

	Linux_dataspace::Filename filename = ds->fname();

	/* pass parent capability as environment variable to the child */
	enum { ENV_STR_LEN = 256 };
	static char envbuf[5][ENV_STR_LEN];
	Genode::snprintf(envbuf[1], ENV_STR_LEN, "parent_local_name=%lu",
	                 _parent.local_name());
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
	char pname_buf[sizeof(_label) + sizeof(prefix)];
	snprintf(pname_buf, sizeof(pname_buf), "%s%s", prefix, _label);
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
	Execve_args arg = { filename.buf, argv_buf, env, _parent.dst().socket };

	_pid = lx_create_process((int (*)(void *))_exec_child,
	                         stack + STACK_SIZE - sizeof(umword_t), &arg);
};
