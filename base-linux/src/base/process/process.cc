/*
 * \brief  Implementation of process creation for Linux
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/elf.h>
#include <base/env.h>
#include <base/process.h>
#include <base/printf.h>
#include <base/snprintf.h>
#include <linux_dataspace/client.h>

/* Framework-internal includes */
#include <linux_syscalls.h>


using namespace Genode;

Dataspace_capability Process::_dynamic_linker_cap;

/**
 * Argument frame for passing 'execve' paremeters through 'clone'
 */
struct execve_args {
	const char *filename;
	char *const*argv;
	char *const*envp;
};


/**
 * Startup code of the new child process
 */
static int _exec_child(struct execve_args *arg)
{
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
	Genode::size_t key_len = strlen(key);
	for (char **curr = lx_environ; curr && *curr; curr++)
		if ((Genode::strcmp(*curr, key, key_len) == 0) && (*curr)[key_len] == '=')
			return (const char *)(*curr + key_len + 1);

	return "";
}

/**
 * Check for dynamic ELF header
 */
static bool _check_dynamic_elf(Dataspace_capability elf_ds_cap)
{
	/* attach ELF locally */
	addr_t elf_addr;

	try { elf_addr = env()->rm_session()->attach(elf_ds_cap); }
	catch (...) { return false; }

	/*
	 * If attach is called within core, it will return zero because
	 * Linux uses Core_rm_session.
	 */
	if (!elf_addr) return false;

	/* read program header and interpreter */
	Elf_binary elf((addr_t)elf_addr);
	env()->rm_session()->detach((void *)elf_addr);

	return elf.is_dynamically_linked();
}


const char *Process::_priv_pd_args(Parent_capability parent_cap,
                                   Dataspace_capability elf_data_ds_cap,
                                   const char *name, char *const argv[])
{
	/*
	 * Serialize calls of this function because it uses the static 'envbuf' and
	 * 'stack' variables.
	 */
	static Lock _priv_pd_args_lock;
	Lock::Guard _lock_guard(_priv_pd_args_lock);
	
	/* check for dynamic program header */
	if (_check_dynamic_elf(elf_data_ds_cap)) {
		if (!_dynamic_linker_cap.valid()) {
			PERR("Dynamically linked file found, but no dynamic linker binary present");
			return 0;
		}
		elf_data_ds_cap = _dynamic_linker_cap;
	}

	/* pass parent capability as environment variable to the child */
	enum { ENV_STR_LEN = 256 };
	static char envbuf[5][ENV_STR_LEN];
	Genode::snprintf(envbuf[0], ENV_STR_LEN, "parent_tid=%ld",
	                 parent_cap.dst().tid);
	Genode::snprintf(envbuf[1], ENV_STR_LEN, "parent_local_name=%lu",
	                 parent_cap.local_name());
	Genode::snprintf(envbuf[2], ENV_STR_LEN, "DISPLAY=%s",
	                 get_env("DISPLAY"));
	Genode::snprintf(envbuf[3], ENV_STR_LEN, "HOME=%s",
	                 get_env("HOME"));
	Genode::snprintf(envbuf[4], ENV_STR_LEN, "LD_LIBRARY_PATH=%s",
	                 get_env("LD_LIBRARY_PATH"));

	char *env[] = { &envbuf[0][0], &envbuf[1][0], &envbuf[2][0],
	                &envbuf[3][0], &envbuf[4][0], 0 };

	/* determine name of binary to start */
	Linux_dataspace_client elf_data_ds(elf_data_ds_cap);
	Linux_dataspace::Filename fname = elf_data_ds.fname();
	fname.buf[sizeof(fname.buf) - 1] = 0;

	/* prefix name of Linux program (helps killing some zombies) */
	char pname_buf[9 + Linux_dataspace::FNAME_LEN];
	snprintf(pname_buf, sizeof(pname_buf), "[Genode] %s", name);

	/* it may happen, that argv is null */
	char *argv_buf[2];
	if (!argv) {
		argv_buf[0] = pname_buf;
		argv_buf[1] = 0;
		argv = argv_buf;
	} else
		((char **)argv)[0] = pname_buf;

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
	struct execve_args arg = {
		fname.buf,
		argv,
		env
	};

	pid_t pid = lx_create_process((int (*)(void *))_exec_child,
	                              stack + STACK_SIZE - sizeof(umword_t), &arg);

	/*
	 * We create a pseudo pd session with the new pd's pid as argument
	 * to enable Core to kill the process when the pd session gets closed.
	 */
	snprintf(_priv_pd_argbuf, sizeof(_priv_pd_argbuf), "PID=%d", pid);

	return _priv_pd_argbuf;
}


Process::Process(Dataspace_capability   elf_data_ds_cap,
                 Ram_session_capability ram_session_cap,
                 Cpu_session_capability cpu_session_cap,
                 Rm_session_capability  rm_session_cap,
                 Parent_capability      parent_cap,
                 const char            *name,
                 char *const            argv[])
:
	_pd(_priv_pd_args(parent_cap, elf_data_ds_cap, name, argv)),
	_cpu_session_client(Cpu_session_capability()),
	_rm_session_client(Rm_session_capability())
{ }


Process::~Process() { }
