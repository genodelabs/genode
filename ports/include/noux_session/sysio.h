/*
 * \brief  Facility for passing system-call arguments
 * \author Norman Feske
 * \date   2011-02-15
 *
 * The 'Sysio' data structure is shared between the noux environment
 * and the child. It is used to pass system-call arguments that would
 * traditionally be transferred via 'copy_from_user' and 'copy_to_user'.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOUX_SESSION__SYSIO_H_
#define _INCLUDE__NOUX_SESSION__SYSIO_H_

/* Genode includes */
#include <util/misc_math.h>


#define SYSIO_DECL(syscall_name, args, results) \
	struct args syscall_name##_in; \
	struct results syscall_name##_out;


namespace Noux {

	struct Sysio
	{
		/*
		 * Information about pending signals
		 */
		enum { SIG_MAX = 32 };
		bool sig_mask[SIG_MAX];

		/*
		 * Number of pending signals
		 */
		int sig_cnt;

		enum { MAX_PATH_LEN = 512 };
		typedef char Path[MAX_PATH_LEN];

		enum { CHUNK_SIZE = 7*1024 };
		typedef char Chunk[CHUNK_SIZE];

		enum { ARGS_MAX_LEN = 3*1024 };
		typedef char Args[ARGS_MAX_LEN];

		enum { ENV_MAX_LEN  = 3*1024 };
		typedef char Env[ENV_MAX_LEN];

		typedef __SIZE_TYPE__ size_t;

		enum {
			STAT_MODE_SYMLINK   = 0120000,
			STAT_MODE_FILE      = 0100000,
			STAT_MODE_DIRECTORY = 0040000,
			STAT_MODE_CHARDEV   = 0020000,
		};

		struct Stat
		{
			Genode::size_t size;
			unsigned       mode;
			unsigned       uid;
			unsigned       gid;
			unsigned long  inode;
			unsigned       device;
		};

		/**
		 * Argument structure used for ioctl syscall
		 */
		struct Ioctl_in
		{
			enum Opcode { OP_UNDEFINED, OP_TIOCGWINSZ };

			Opcode request;
		};

		/**
		 * Structure carrying the result values of 'ioctl' syscalls
		 */
		struct Ioctl_out
		{
			union
			{
				/* if request was 'OP_TIOCGSIZE' */
				struct {
					int rows;
					int columns;
				} tiocgwinsz;
			};
		};

		enum { DIRENT_MAX_NAME_LEN = 128 };

		enum Dirent_type {
			DIRENT_TYPE_FILE,
			DIRENT_TYPE_DIRECTORY,
			DIRENT_TYPE_FIFO,
			DIRENT_TYPE_SYMLINK,
			DIRENT_TYPE_END
		};

		struct Dirent
		{
			int         fileno;
			Dirent_type type;
			char        name[DIRENT_MAX_NAME_LEN];
		};

		enum Fcntl_cmd {
			FCNTL_CMD_GET_FILE_STATUS_FLAGS,
			FCNTL_CMD_SET_FD_FLAGS
		};

		/**
		 * Input and output argument type of select syscall
		 */
		struct Select_fds
		{
			/**
			 * Maximum number of file descriptors supported
			 */
			enum { MAX_FDS = 32U };

			/**
			 * Number of file descriptors to watch for read operations (rd),
			 * write operations (wr), or exceptions (ex).
			 */
			size_t num_rd,
			       num_wr,
			       num_ex;

			/**
			 * Array containing the file descriptors, starting with those
			 * referring to rd, followed by wr, and finally ex
			 */
			int array[MAX_FDS];

			/**
			 * Return total number of file descriptors contained in the array
			 */
			size_t total_fds() const {
				return Genode::min(num_rd + num_wr + num_ex, (size_t)MAX_FDS); }

			/**
			 * Check for maximum population of fds array
			 */
			bool max_fds_exceeded() const
			{
				/*
				 * Note that even though the corner case of num_rd + num_wr +
				 * num_ex == MAX_FDS is technically valid, this condition hints
				 * at a possible attempt to over popupate the array (see the
				 * implementation of 'select' in the Noux libc plugin). Hence,
				 * we regard this case as an error, too.
				 */
				return total_fds() >= MAX_FDS;
			}

			/**
			 * Return true of fd set index should be watched for reading
			 */
			bool watch_for_rd(unsigned i) const { return i < num_rd; }

			/**
			 * Return true if fd set index should be watched for writing
			 */
			bool watch_for_wr(unsigned i) const {
				return (i >= num_rd) && (i < num_rd + num_wr); }

			/**
			 * Return true if fd set index should be watched for exceptions
			 */
			bool watch_for_ex(unsigned i) const {
				return (i >= num_rd + num_wr) && (i < total_fds()); }
		};

		struct Select_timeout
		{
			long sec, usec;

			/**
			 * Set timeout to infinity
			 */
			void set_infinite() { sec = -1; usec = -1; }

			/**
			 * Return true if the timeout is infinite
			 */
			bool infinite() const { return (sec == -1) && (usec == -1); }

			/**
			 * Return true if the timeout is zero
			 */
			bool zero() const { return (sec == 0) && (usec == 0); }
		};

		enum General_error { ERR_FD_INVALID, NUM_GENERAL_ERRORS };
		enum Stat_error    { STAT_ERR_NO_ENTRY     = NUM_GENERAL_ERRORS };
		enum Fcntl_error   { FCNTL_ERR_CMD_INVALID = NUM_GENERAL_ERRORS };
		enum Open_error    { OPEN_ERR_UNACCESSIBLE = NUM_GENERAL_ERRORS };
		enum Execve_error  { EXECVE_NONEXISTENT    = NUM_GENERAL_ERRORS };

		union {
			General_error general;
			Stat_error    stat;
			Fcntl_error   fcntl;
			Open_error    open;
			Execve_error  execve;
		} error;

		union {

			SYSIO_DECL(getcwd, { }, { Path path; });

			SYSIO_DECL(write,  { int fd; size_t count; Chunk chunk; }, { });

			SYSIO_DECL(stat,   { Path path; }, { Stat st; });

			SYSIO_DECL(fstat,  { int fd; }, { Stat st; });

			SYSIO_DECL(fcntl,  { int fd; long long_arg; Fcntl_cmd cmd; },
			                   { int result; });

			SYSIO_DECL(open,   { Path path; int mode; }, { int fd; });

			SYSIO_DECL(close,  { int fd; }, { });

			SYSIO_DECL(ioctl,  : Ioctl_in { int fd; }, : Ioctl_out { });

			SYSIO_DECL(dirent, { int fd; int index; }, { Dirent entry; });

			SYSIO_DECL(fchdir, { int fd; }, { });

			SYSIO_DECL(read,   { int fd; Genode::size_t count; },
			                   { Chunk chunk; size_t count; });

			SYSIO_DECL(execve, { Path filename; Args args; Env env; }, { });

			SYSIO_DECL(select, { Select_fds fds; Select_timeout timeout; },
			                   { Select_fds fds; });

			SYSIO_DECL(fork,   { Genode::addr_t ip; Genode::addr_t sp;
			                     Genode::addr_t parent_cap_addr; },
			                   { int pid; });

			SYSIO_DECL(getpid, { }, { int pid; });

			SYSIO_DECL(wait4,  { int pid; bool nohang; },
			                   { int pid; int status; });
			SYSIO_DECL(pipe,   { }, { int fd[2]; });

			SYSIO_DECL(dup2,   { int fd; int to_fd; }, { });
		};
	};
};

#undef SYSIO_DECL

#endif /* _INCLUDE__NOUX_SESSION__SYSIO_H_ */
