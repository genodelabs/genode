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

	using namespace Genode;

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
		typedef long int ssize_t;

		/**
		 * Flags of 'mode' argument of open syscall
		 */
		enum {
			OPEN_MODE_RDONLY  = 0,
			OPEN_MODE_WRONLY  = 1,
			OPEN_MODE_RDWR    = 2,
			OPEN_MODE_ACCMODE = 3,
			OPEN_MODE_CREATE  = 0x0800, /* libc O_EXCL */
		};

		enum {
			STAT_MODE_SYMLINK   = 0120000,
			STAT_MODE_FILE      = 0100000,
			STAT_MODE_DIRECTORY = 0040000,
			STAT_MODE_CHARDEV   = 0020000,
		};

		struct Stat
		{
			size_t         size;
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
			enum Opcode { OP_UNDEFINED, OP_TIOCGWINSZ, OP_FIONBIO };

			Opcode request;
			int argp;
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

		enum Lseek_whence { LSEEK_SET, LSEEK_CUR, LSEEK_END };

		enum { DIRENT_MAX_NAME_LEN = 128 };

		enum Dirent_type {
			DIRENT_TYPE_FILE,
			DIRENT_TYPE_DIRECTORY,
			DIRENT_TYPE_FIFO,
			DIRENT_TYPE_CHARDEV,
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
			FCNTL_CMD_SET_FILE_STATUS_FLAGS,
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
				return min(num_rd + num_wr + num_ex, (size_t)MAX_FDS); }

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

		/**
		 * Socket related structures
		 */
		enum { MAX_HOSTNAME_LEN = 255 };
		typedef char Hostname[MAX_HOSTNAME_LEN];

		enum { MAX_SERVNAME_LEN = 255 };
		typedef char Servname[MAX_SERVNAME_LEN];

		enum { MAX_ADDRINFO_RESULTS = 4 };

		struct in_addr
		{
			unsigned int    s_addr;
		};

		struct sockaddr
		{
			unsigned char   sa_len;
			unsigned char   sa_family;
			char            sa_data[14];
		};

		struct sockaddr_in {
			unsigned char   sin_len;
			unsigned char   sin_family;
			unsigned short  sin_port;
			struct          in_addr sin_addr;
			char            sin_zero[8];
		};

		typedef unsigned        socklen_t;

		struct addrinfo {
			int              ai_flags;
			int              ai_family;
			int              ai_socktype;
			int              ai_protocol;
			socklen_t        ai_addrlen;
			struct sockaddr *ai_addr;
			char            *ai_canonname;
			struct addrinfo *ai_next;
		};

		struct Addrinfo {
			struct addrinfo  addrinfo;
			struct sockaddr  ai_addr;
			char             ai_canonname[255];
		};

		/**
		 * user info defintions
		 */
		enum { USERINFO_GET_ALL = 0, USERINFO_GET_UID, USERINFO_GET_GID };
		enum { MAX_USERNAME_LEN = 32 };
		typedef char User[MAX_USERNAME_LEN];
		enum { MAX_SHELL_LEN = 16 };
		typedef char Shell[MAX_SHELL_LEN];
		enum { MAX_HOME_LEN = 128 };
		typedef char Home[MAX_HOME_LEN];
		typedef unsigned int Uid;

		enum General_error   { ERR_FD_INVALID, NUM_GENERAL_ERRORS };
		enum Stat_error      { STAT_ERR_NO_ENTRY     = NUM_GENERAL_ERRORS };
		enum Fchdir_error    { FCHDIR_ERR_NOT_DIR    = NUM_GENERAL_ERRORS };
		enum Fcntl_error     { FCNTL_ERR_CMD_INVALID = NUM_GENERAL_ERRORS };
		enum Ftruncate_error { FTRUNCATE_ERR_NO_PERM = NUM_GENERAL_ERRORS };
		enum Open_error      { OPEN_ERR_UNACCESSIBLE, OPEN_ERR_NO_PERM,
		                       OPEN_ERR_EXISTS };
		enum Execve_error    { EXECVE_NONEXISTENT    = NUM_GENERAL_ERRORS };
		enum Unlink_error    { UNLINK_ERR_NO_ENTRY, UNLINK_ERR_NO_PERM };
		enum Rename_error    { RENAME_ERR_NO_ENTRY, RENAME_ERR_CROSS_FS,
		                       RENAME_ERR_NO_PERM };
		enum Mkdir_error     { MKDIR_ERR_EXISTS,   MKDIR_ERR_NO_ENTRY,
		                       MKDIR_ERR_NO_SPACE, MKDIR_ERR_NO_PERM,
		                       MKDIR_ERR_NAME_TOO_LONG};

		union {
			General_error   general;
			Stat_error      stat;
			Fchdir_error    fchdir;
			Fcntl_error     fcntl;
			Ftruncate_error ftruncate;
			Open_error      open;
			Execve_error    execve;
			Unlink_error    unlink;
			Rename_error    rename;
			Mkdir_error     mkdir;
		} error;

		union {

			SYSIO_DECL(getcwd,      { }, { Path path; });

			SYSIO_DECL(write,       { int fd; size_t count; Chunk chunk; },
			                        { size_t count; });

			SYSIO_DECL(stat,        { Path path; }, { Stat st; });

			SYSIO_DECL(fstat,       { int fd; }, { Stat st; });

			SYSIO_DECL(ftruncate,   { int fd; off_t length; }, { });

			SYSIO_DECL(fcntl,       { int fd; long long_arg; Fcntl_cmd cmd; },
			                        { int result; });

			SYSIO_DECL(open,        { Path path; int mode; }, { int fd; });

			SYSIO_DECL(close,       { int fd; }, { });

			SYSIO_DECL(ioctl,       : Ioctl_in { int fd; }, : Ioctl_out { });

			SYSIO_DECL(lseek,       { int fd; off_t offset; Lseek_whence whence; },
			                        { off_t offset; });

			SYSIO_DECL(dirent,      { int fd; }, { Dirent entry; });

			SYSIO_DECL(fchdir,      { int fd; }, { });

			SYSIO_DECL(read,        { int fd; size_t count; },
			                        { Chunk chunk; size_t count; });

			SYSIO_DECL(execve,      { Path filename; Args args; Env env; }, { });

			SYSIO_DECL(select,      { Select_fds fds; Select_timeout timeout; },
			                        { Select_fds fds; });

			SYSIO_DECL(fork,        { addr_t ip; addr_t sp;
			                          addr_t parent_cap_addr; },
			                        { int pid; });

			SYSIO_DECL(getpid,      { }, { int pid; });

			SYSIO_DECL(wait4,       { int pid; bool nohang; },
			                        { int pid; int status; });
			SYSIO_DECL(pipe,        { }, { int fd[2]; });

			SYSIO_DECL(dup2,        { int fd; int to_fd; }, { });

			SYSIO_DECL(unlink,      { Path path; }, { });

			SYSIO_DECL(rename,      { Path from_path; Path to_path; }, { });

			SYSIO_DECL(mkdir,       { Path path; int mode; }, { });

			SYSIO_DECL(socket,      { int domain; int type; int protocol; },
			                        { int fd; });

			 /* XXX for now abuse Chunk for passing optval */
			SYSIO_DECL(getsockopt,  { int fd; int level; int optname; Chunk optval;
			                          socklen_t optlen; }, { int result; });

			SYSIO_DECL(setsockopt,  { int fd; int level;
			                          int optname; Chunk optval;
			                          socklen_t optlen; }, { });

			SYSIO_DECL(accept,      { int fd; struct sockaddr addr; socklen_t addrlen; },
			                        { int fd; });

			SYSIO_DECL(bind,        { int fd; struct sockaddr addr;
			                          socklen_t addrlen; }, { int result; });

			SYSIO_DECL(getpeername, { int fd; struct sockaddr addr;
			                          socklen_t addrlen; }, { });

			SYSIO_DECL(listen,      { int fd; int type; int backlog; },
			                        { int result; });

			SYSIO_DECL(send,        { int fd; Chunk buf; size_t len; int flags; },
			                        { ssize_t len; });

			SYSIO_DECL(sendto,      { int fd; Chunk buf; size_t len; int flags;
			                          struct sockaddr dest_addr; socklen_t addrlen; },
			                        { ssize_t len; });

			SYSIO_DECL(recv,        { int fd; Chunk buf; size_t len; int flags; },
			                        { size_t len; });

			SYSIO_DECL(recvfrom,    { int fd; Chunk buf; size_t len; int flags;
			                          struct sockaddr src_addr; socklen_t addrlen; },
			                        { size_t len; });

			SYSIO_DECL(shutdown,    { int fd; int how; }, { });

			SYSIO_DECL(connect,     { int fd; struct sockaddr addr; socklen_t addrlen; },
			                        { int result; });

			SYSIO_DECL(userinfo, { int request; Uid uid; },
			                     { User name; Uid uid; Uid gid; Shell shell;
			                       Home home; });
		};
	};
};

#undef SYSIO_DECL

#endif /* _INCLUDE__NOUX_SESSION__SYSIO_H_ */
