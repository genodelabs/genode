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
#include <base/printf.h>
#include <base/snprintf.h>

/* core-local includes */
#include <pd_session_component.h>
#include <dataspace_component.h>

/* Linux includes */
#include <core_linux_syscalls.h>

using namespace Genode;


/***********************************
 ** Utilities for chroot handling **
 ***********************************/

enum { MAX_PATH_LEN = 256 };


/**
 * Return true if specified path is an existing directory
 */
static bool is_directory(char const *path)
{
	struct stat64 s;
	if (lx_stat(path, &s) != 0)
		return false;

	if (!(s.st_mode & S_IFDIR))
		return false;

	return true;
}


static bool is_path_delimiter(char c) { return c == '/'; }


static bool has_trailing_path_delimiter(char const *path)
{
	char last_char = 0;
	for (; *path; path++)
		last_char = *path;

	return is_path_delimiter(last_char);
}


/**
 * Return number of path elements of given path
 */
static Genode::size_t num_path_elements(char const *path)
{
	Genode::size_t count = 0;

	/*
	 * If path starts with non-slash, the first characters belongs to a path
	 * element.
	 */
	if (*path && !is_path_delimiter(*path))
		count = 1;

	/* count slashes */
	for (; *path; path++)
		if (is_path_delimiter(*path))
			count++;

	return count;
}


static bool leading_path_elements(char const *path, unsigned num,
                                  char *dst, Genode::size_t dst_len)
{
	/* counter of path delimiters */
	unsigned count = 0;
	unsigned i     = 0;

	if (is_path_delimiter(path[0]))
		num++;

	for (; path[i] && (count < num) && (i < dst_len); i++)
	{
		if (is_path_delimiter(path[i]))
			count++;

		if (count == num)
			break;

		dst[i] = path[i];
	}

	if (i + 1 < dst_len) {
		dst[i] = 0;
		return true;
	}

	/* string is cut, append null termination anyway */
	dst[dst_len - 1] = 0;
	return false;
}


static void mirror_path_to_chroot(char const *chroot_path, char const *path)
{
	char target_path[MAX_PATH_LEN];
	Genode::snprintf(target_path, sizeof(target_path), "%s%s",
	                 chroot_path, path);

	/*
	 * Create directory hierarchy pointing to the target path except for the
	 * last element. The last element will be bind-mounted to refer to the
	 * original 'path'.
	 */
	for (unsigned i = 1; i <= num_path_elements(target_path); i++)
	{
		char buf[MAX_PATH_LEN];
		leading_path_elements(target_path, i, buf, sizeof(buf));

		/* skip existing directories */
		if (is_directory(buf))
			continue;

		/* create new directory */
		lx_mkdir(buf, 0777);
	}

	lx_umount(target_path);

	int ret = 0;
	if ((ret = lx_bindmount(path, target_path)))
		PERR("bind mount failed (errno=%d)", ret);
}


/**
 * Setup content of chroot environment as prerequisite to 'execve' new
 * processes within the environment. I.e., the current working directory
 * containing the ROM modules must be mounted at the same location within the
 * chroot environment.
 */
static bool setup_chroot_environment(char const *chroot_path)
{
	using namespace Genode;

	static char cwd_path[MAX_PATH_LEN];

	lx_getcwd(cwd_path, sizeof(cwd_path));

	/*
	 * Validate chroot path
	 */
	if (!is_directory(chroot_path)) {
		PERR("chroot path does not point to valid directory");
		return false;
	}

	if (has_trailing_path_delimiter(chroot_path)) {
		PERR("chroot path has trailing slash");
		return false;
	}

	/*
	 * Hardlink directories needed for running Genode within the chroot
	 * environment.
	 */
	mirror_path_to_chroot(chroot_path, cwd_path);

	return true;
}


/***************
 ** Utilities **
 ***************/

/**
 * Argument frame for passing 'execve' paremeters through 'clone'
 */
struct Execve_args
{
	char         const *filename;
	char         const *root;
	char       * const *argv;
	char       * const *envp;
	unsigned int const  uid;
	unsigned int const  gid;
	int          const parent_sd;

	Execve_args(char   const *filename,
	            char   const *root,
	            char * const *argv,
	            char * const *envp,
	            unsigned int  uid,
	            unsigned int  gid,
	            int           parent_sd)
	:
		filename(filename), root(root), argv(argv), envp(envp),
		uid(uid), gid(gid), parent_sd(parent_sd)
	{ }
};


/**
 * Startup code of the new child process
 */
static int _exec_child(Execve_args *arg)
{
	lx_dup2(arg->parent_sd, PARENT_SOCKET_HANDLE);

	/* change to chroot environment */
	if (arg->root && arg->root[0]) {
		char cwd[1024];

		PDBG("arg->root='%s'", arg->root);

		if (setup_chroot_environment(arg->root) == false) {
			PERR("Could not setup chroot environment");
			return -1;
		}

		if (!lx_getcwd(cwd, sizeof(cwd))) {
			PERR("Failed to getcwd");
			return -1;
		}

		PLOG("changing root of %s (PID %d) to %s",
		     arg->filename, lx_getpid(), arg->root);

		int ret = lx_chroot(arg->root);
		if (ret < 0) {
			PERR("Syscall chroot failed (errno %d)", ret);
			return -1;
		}

		ret = lx_chdir(cwd);
		if (ret < 0) {
			PERR("chdir to new chroot failed");
			return -1;
		}
	}

	/*
	 * Set UID and GID
	 *
	 * We must set the GID prior setting the UID because setting the GID won't
	 * be possible anymore once we set the UID to non-root.
	 */
	if (arg->gid) {
		int const ret = lx_setgid(arg->gid);
		if (ret)
			PWRN("Could not set PID %d (%s) to GID %u (error %d)",
			     lx_getpid(), arg->filename, arg->gid, ret);
	}
	if (arg->uid) {
		int const ret = lx_setuid(arg->uid);
		if (ret)
			PWRN("Could not set PID %d (%s) to UID %u (error %d)",
			     lx_getpid(), arg->filename, arg->uid, ret);
	}

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
	_pid(0), _uid(0), _gid(0), _ds_ep(ep)
{
	Arg_string::find_arg(args, "label").string(_label, sizeof(_label),
	                                           "<unlabeled>");

	/*
	 * Read Linux-specific session arguments
	 */
	Arg_string::find_arg(args, "root").string(_root, sizeof(_root), "");

	_uid = Arg_string::find_arg(args, "uid").ulong_value(0);
	_gid = Arg_string::find_arg(args, "gid").ulong_value(0);

	bool const is_chroot = (Genode::strcmp(_root, "") != 0);

	/*
	 * If a UID is specified but no GID, we use the UID as GID. This way, a
	 * configuration error where the UID is defined but the GID is left
	 * undefined won't result in the execution of the new process with the
	 * root user's GID.
	 */
	if (_gid == 0)
		_gid = _uid;

	/*
	 * Print Linux-specific session arguments if specified
	 *
	 * This output used for the automated 'lx_pd_args' test.
	 */
	if (is_chroot || _uid || _gid)
		printf("PD session for '%s'\n", _label);

	if (is_chroot) printf("  root: %s\n", _root);
	if (_uid)      printf("  uid:  %u\n", _uid);
	if (_gid)      printf("  gid:  %u\n", _gid);
}


Pd_session_component::~Pd_session_component()
{
	if (_pid)
		lx_kill(_pid, 9);
}


int Pd_session_component::bind_thread(Thread_capability) { return -1; }


int Pd_session_component::assign_parent(Parent_capability parent)
{
	_parent = parent;
	return 0;
}


void Pd_session_component::start(Capability<Dataspace> binary)
{
	const char *tmp_filename = "temporary_executable_elf_dataspace_file_for_execve";

	/* lookup binary dataspace */
	Object_pool<Dataspace_component>::Guard ds(_ds_ep->lookup_and_lock(binary));

	if (!ds) {
		PERR("could not lookup binary, aborted PD startup");
		return; /* XXX reflect error to client */
	}

	const char *filename = ds->fname().buf;

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
			PERR("Could not create file '%s'", filename);
			return; /* XXX reflect error to client */
		}

		char buf[4096];
		int num_bytes = 0;
		while ((num_bytes = lx_read(ds->fd().dst().socket, buf, sizeof(buf))) != 0)
			lx_write(tmp_binary_fd, buf, num_bytes);

		lx_close(tmp_binary_fd);
	}

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
	Execve_args arg(filename, _root, argv_buf, env, _uid, _gid,
	                _parent.dst().socket);

	_pid = lx_create_process((int (*)(void *))_exec_child,
	                         stack + STACK_SIZE - sizeof(umword_t), &arg);

	if (strcmp(filename, tmp_filename) == 0)
		lx_unlink(filename);
};
