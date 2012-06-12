/*
 * \brief  Noux libc plugin
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/misc_math.h>
#include <util/arg_string.h>
#include <base/printf.h>
#include <rom_session/connection.h>
#include <base/sleep.h>
#include <dataspace/client.h>

/* noux includes */
#include <noux_session/connection.h>
#include <noux_session/sysio.h>

/* libc plugin includes */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>


enum { verbose = false };


void *operator new (size_t, void *ptr) { return ptr; }


class Noux_connection
{
	private:

		Noux::Connection _connection;
		Noux::Sysio      *_sysio;

		Noux::Sysio *_obtain_sysio()
		{
			return Genode::env()->rm_session()->attach(_connection.sysio_dataspace());
		}

	public:

		Noux_connection() : _sysio(_obtain_sysio()) { }

		void reconnect()
		{
			new (&_connection) Noux_connection;
			Genode::env()->rm_session()->detach(_sysio);
			_sysio = _obtain_sysio();
		}

		Noux::Session *session() { return &_connection; }
		Noux::Sysio   *sysio()   { return  _sysio; }
};


Noux_connection *noux_connection()
{
	static Noux_connection inst;
	return &inst;
}


Noux::Session *noux()  { return noux_connection()->session(); }
Noux::Sysio   *sysio() { return noux_connection()->sysio();   }


enum { FS_BLOCK_SIZE = 1024 };


/***********************************************
 ** Overrides of libc default implementations **
 ***********************************************/

extern "C" int __getcwd(char *dst, Genode::size_t dst_size)
{
	noux()->syscall(Noux::Session::SYSCALL_GETCWD);
	Genode::size_t path_size = Genode::strlen(sysio()->getcwd_out.path);

	if (dst_size < path_size + 1)
		return -ERANGE;

	Genode::strncpy(dst, sysio()->getcwd_out.path, dst_size);
	return 0;
}


/**
 * Utility to copy-out syscall results to buf struct
 *
 * Code shared between 'stat' and 'fstat'.
 */
static void _sysio_to_stat_struct(Noux::Sysio const *sysio, struct stat *buf)
{
	Genode::memset(buf, 0, sizeof(*buf));
	buf->st_uid     = sysio->stat_out.st.uid;
	buf->st_gid     = sysio->stat_out.st.gid;
	buf->st_mode    = sysio->stat_out.st.mode;
	buf->st_size    = sysio->stat_out.st.size;
	buf->st_blksize = FS_BLOCK_SIZE;
	buf->st_blocks  = (buf->st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
	buf->st_ino     = sysio->stat_out.st.inode;
	buf->st_dev     = sysio->stat_out.st.device;
}


static int _stat(char const *path, struct stat *buf, bool lstat = false)
{
	if ((path == NULL) or (buf == NULL)) {
		errno = EFAULT;
		return -1;
	}

	Genode::strncpy(sysio()->stat_in.path, path, sizeof(sysio()->stat_in.path));

	if (!noux()->syscall(Noux::Session::SYSCALL_STAT)) {
		PWRN("stat syscall failed for path \"%s\"", path);
		switch (sysio()->error.stat) {
		case Noux::Sysio::STAT_ERR_NO_ENTRY: errno = ENOENT; break;
		}
		return -1;
	}

	_sysio_to_stat_struct(sysio(), buf);
	return 0;
}


extern "C" int lstat(char const *path, struct stat *buf) { return _stat(path, buf, true);  }


static bool serialize_string_array(char const * const * array, char *dst, Genode::size_t dst_len)
{
	for (unsigned i = 0; array[i]; i++)
	{
		Genode::size_t const curr_len = Genode::strlen(array[i]) + 1;
		if (curr_len + 1 >= dst_len)
			return false;

		Genode::strncpy(dst, array[i], dst_len);

		dst += curr_len;
		dst_len -= curr_len;
	}

	dst[0] = 0;
	return true;
}


extern "C" int execve(char const *filename, char *const argv[],
                      char *const envp[])
{
	if (verbose) {
		PDBG("filename=%s", filename);

		for (int i = 0; argv[i]; i++)
			PDBG("argv[%d]='%s'", i, argv[i]);

		for (int i = 0; envp[i]; i++)
			PDBG("envp[%d]='%s'", i, envp[i]);
	}

	Genode::strncpy(sysio()->execve_in.filename, filename, sizeof(sysio()->execve_in.filename));
	if (!serialize_string_array(argv, sysio()->execve_in.args,
	                           sizeof(sysio()->execve_in.args))) {
	    PERR("execve: argument buffer exceeded");
	    errno = E2BIG;
	    return -1;
	}

	if (!serialize_string_array(envp, sysio()->execve_in.env,
	                            sizeof(sysio()->execve_in.env))) {
	    PERR("execve: environment buffer exceeded");
	    errno = E2BIG;
	    return -1;
	}

	if (!noux()->syscall(Noux::Session::SYSCALL_EXECVE)) {
		PWRN("exec syscall failed for path \"%s\"", filename);
		switch (sysio()->error.execve) {
		case Noux::Sysio::EXECVE_NONEXISTENT: errno = ENOENT; break;
		}
		return -1;
	}

	/*
	 * In the success case, we never return from execve, the execution is
	 * resumed in the new program.
	 */
	Genode::sleep_forever();
	return 0;
}


/**
 * Called by execvp
 */
extern "C" int _execve(char const *filename, char *const argv[],
                       char *const envp[])
{
	return execve(filename, argv, envp);
}


/**
 * Return number of marhalled file descriptors into select argument buffer
 *
 * \return number of marshalled file descriptors, this value is guaranteed to
 *         not exceed the 'dst_fds_len' argument
 */
static size_t marshal_fds(fd_set *src_fds, int nfds,
                       int *dst_fds, size_t dst_fds_len)
{
	if (!src_fds) return 0;

	size_t num_fds = 0;

	for (int fd = 0; (fd < nfds) && (num_fds < dst_fds_len); fd++)
		if (FD_ISSET(fd, src_fds))
			dst_fds[num_fds++] = fd;

	return num_fds;
}


/**
 * Unmarshal result of select syscall into fd set
 */
static void unmarshal_fds(int *src_fds, size_t src_fds_len, fd_set *dst_fds)
{
	if (!dst_fds) return;

	FD_ZERO(dst_fds);

	for (size_t i = 0; i < src_fds_len; i++)
		FD_SET(src_fds[i], dst_fds);
}


extern "C" int select(int nfds, fd_set *readfds, fd_set *writefds,
                      fd_set *exceptfds, struct timeval *timeout)
{

	/*
	 * Marshal file descriptor into sysio page
	 */
	Noux::Sysio::Select_fds &in_fds = sysio()->select_in.fds;

	int   *dst     = in_fds.array;
	size_t dst_len = Noux::Sysio::Select_fds::MAX_FDS;

	in_fds.num_rd = marshal_fds(readfds, nfds, dst, dst_len);

	dst     += in_fds.num_rd;
	dst_len -= in_fds.num_rd;

	in_fds.num_wr = marshal_fds(writefds, nfds, dst, dst_len);

	dst     += in_fds.num_wr;
	dst_len -= in_fds.num_wr;

	in_fds.num_ex = marshal_fds(exceptfds, nfds, dst, dst_len);

	if (in_fds.max_fds_exceeded()) {
		errno = ENOMEM;
		return -1;
	}

	/*
	 * Marshal timeout
	 */
	if (timeout) {
		sysio()->select_in.timeout.sec  = timeout->tv_sec;
		sysio()->select_in.timeout.usec = timeout->tv_usec;

		if (!sysio()->select_in.timeout.zero()) {
//			PDBG("timeout=%d,%d -> replaced by zero timeout",
//			     (int)timeout->tv_sec, (int)timeout->tv_usec);

			sysio()->select_in.timeout.sec  = 0;
			sysio()->select_in.timeout.usec = 0;
		}
	} else {
		sysio()->select_in.timeout.set_infinite();
	}

	/*
	 * Perform syscall
	 */
	if (!noux()->syscall(Noux::Session::SYSCALL_SELECT)) {
		PWRN("select syscall failed");
//		switch (sysio()->error.select) {
//		case Noux::Sysio::SELECT_NONEXISTENT: errno = ENOENT; break;
//		}
		return -1;
	}

	/*
	 * Unmarshal file selectors reported by the select syscall
	 */
	Noux::Sysio::Select_fds &out_fds = sysio()->select_out.fds;

	int *src = out_fds.array;

	unmarshal_fds(src, out_fds.num_rd, readfds);
	src += out_fds.num_rd;

	unmarshal_fds(src, out_fds.num_wr, writefds);
	src += out_fds.num_wr;

	unmarshal_fds(src, out_fds.num_ex, exceptfds);

	return out_fds.total_fds();
}

#include <setjmp.h>
#include <base/platform_env.h>


static jmp_buf fork_jmp_buf;
static Genode::Capability<Genode::Parent>::Raw new_parent;

extern "C" void stdout_reconnect(); /* provided by 'log_console.cc' */


/*
 * The new process created via fork will start its execution here.
 */
extern "C" void fork_trampoline()
{
	static_cast<Genode::Platform_env *>(Genode::env())
		->reload_parent_cap(new_parent.dst, new_parent.local_name);

	stdout_reconnect();
	noux_connection()->reconnect();

	longjmp(fork_jmp_buf, 1);
}


extern "C" pid_t fork(void)
{
	/* stack used for executing 'fork_trampoline' */
	enum { STACK_SIZE = 1024 };
	static long stack[STACK_SIZE];

	if (setjmp(fork_jmp_buf)) {

		/*
		 * We got here via longjmp from 'fork_trampoline'.
		 */
		return 0;

	} else {

		/* got here during the normal control flow of the fork call */
		sysio()->fork_in.ip              = (Genode::addr_t)(&fork_trampoline);
		sysio()->fork_in.sp              = (Genode::addr_t)(&stack[STACK_SIZE]);
		sysio()->fork_in.parent_cap_addr = (Genode::addr_t)(&new_parent);

		if (!noux()->syscall(Noux::Session::SYSCALL_FORK)) {
			PERR("fork error %d", sysio()->error.general);
		}

		return sysio()->fork_out.pid;
	}
}


extern "C" pid_t vfork(void) { return fork(); }


extern "C" pid_t getpid(void)
{
	noux()->syscall(Noux::Session::SYSCALL_GETPID);
	return sysio()->getpid_out.pid;
}


extern "C" int access(char const *pathname, int mode)
{
	if (verbose)
		PDBG("access '%s' (mode=%x) called, not implemented", pathname, mode);

	struct stat stat;
	if (::stat(pathname, &stat) == 0)
		return 0;

	errno = ENOENT;
	return -1;
}


extern "C" int chmod(char const *path, mode_t mode)
{
	if (verbose)
		PDBG("chmod '%s' to 0x%x not implemented", path, mode);
	return 0;
}


extern "C" pid_t _wait4(pid_t pid, int *status, int options,
                        struct rusage *rusage)
{
	sysio()->wait4_in.pid    = pid;
	sysio()->wait4_in.nohang = !!(options & WNOHANG);
	if (!noux()->syscall(Noux::Session::SYSCALL_WAIT4)) {
		PERR("wait4 error %d", sysio()->error.general);
		return -1;
	}

	if (status)
		*status = sysio()->wait4_out.status;

	return sysio()->wait4_out.pid;
}


/********************
 ** Time functions **
 ********************/

/*
 * The default implementations as provided by the libc rely on a dedicated
 * thread. But on Noux, no thread other than the main thread is allowed. For
 * this reason, we need to override the default implementations here.
 */

extern "C" int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	if (verbose)
		PDBG("clock_gettime called - not implemented");
	errno = EINVAL;
	return -1;
}


extern "C" int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (verbose)
		PDBG("gettimeofdaye called - not implemented");
	errno = EINVAL;
	return -1;
}


/*********************
 ** Signal handling **
 *********************/

extern "C" int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	/* XXX todo */
	errno = ENOSYS;
	return -1;
}


extern "C" int _sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	return sigprocmask(how, set, oldset);
}


extern "C" int sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact)
{
	/* XXX todo */
	errno = ENOSYS;
	return -1;
}


/*********************
 ** File operations **
 *********************/

static int noux_fd(Libc::Plugin_context *context)
{
	/*
	 * We use the 'context' pointer only as container for an int value. It is
	 * never used as a pointer. To make 64-bit compilers happy, we need to keep
	 * the bit width of the cast intact. The upper 32 bit are discarded when
	 * leaving the function.
	 */
	return (long)context;
}


static Libc::Plugin_context *noux_context(int noux_fd)
{
	return reinterpret_cast<Libc::Plugin_context*>(noux_fd);
}


namespace {

	class Plugin : public Libc::Plugin
	{
		private:

			Libc::File_descriptor *_stdin;
			Libc::File_descriptor *_stdout;
			Libc::File_descriptor *_stderr;

		public:

			/**
			 * Constructor
			 */
			Plugin() :
				_stdin (Libc::file_descriptor_allocator()->alloc(this, noux_context(0), 0)),
				_stdout(Libc::file_descriptor_allocator()->alloc(this, noux_context(1), 1)),
				_stderr(Libc::file_descriptor_allocator()->alloc(this, noux_context(2), 2))
			{ }

			bool supports_chdir(char const *)                { return true; }
			bool supports_open(char const *, int)            { return true; }
			bool supports_stat(char const *)                 { return true; }
			bool supports_pipe()                             { return true; }
			bool supports_unlink(char const *)               { return true; }
			bool supports_rename(const char *, const char *) { return true; }
			bool supports_mkdir(const char *, mode_t)        { return true; }
			bool supports_socket(int, int, int)              { return true; }
			bool supports_freeaddrinfo(struct addrinfo *)    { return true; }
			bool supports_getaddrinfo(const char *, const char *,
						  struct addrinfo **)    { return true; }

			Libc::File_descriptor *open(char const *, int);
			ssize_t write(Libc::File_descriptor *, const void *, ::size_t);
			int close(Libc::File_descriptor *);
			int dup2(Libc::File_descriptor *, Libc::File_descriptor *);
			int fstat(Libc::File_descriptor *, struct stat *);
			int fsync(Libc::File_descriptor *);
			int fstatfs(Libc::File_descriptor *, struct statfs *);
			int fcntl(Libc::File_descriptor *, int, long);
			ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t, ::off_t *);
			::off_t lseek(Libc::File_descriptor *, ::off_t offset, int whence);
			int fchdir(Libc::File_descriptor *);
			ssize_t read(Libc::File_descriptor *, void *, ::size_t);
			int stat(char const *, struct stat *);
			int ioctl(Libc::File_descriptor *, int request, char *argp);
			int pipe(Libc::File_descriptor *pipefd[2]);
			int unlink(char const *path);
			int rename(const char *oldpath, const char *newpath);
			int mkdir(const char *path, mode_t mode);

			/* Network related functions */
			Libc::File_descriptor *socket(int, int, int);
			Libc::File_descriptor *accept(Libc::File_descriptor *,
						      struct sockaddr *, socklen_t *);
			int bind(Libc::File_descriptor *, const struct sockaddr *,
				 socklen_t);
			int connect(Libc::File_descriptor *, const struct sockaddr *addr,
				    socklen_t addrlen);
			void freeaddrinfo(struct addrinfo *);
			int getaddrinfo(const char *, const char *, const struct addrinfo *,
					struct addrinfo **);
			int getpeername(Libc::File_descriptor *, struct sockaddr *,
					socklen_t *);
			int listen(Libc::File_descriptor *, int);
			ssize_t send(Libc::File_descriptor *, const void *, ::size_t,
				     int flags);
			ssize_t sendto(Libc::File_descriptor *, const void *, size_t, int,
				       const struct sockaddr *, socklen_t);
			ssize_t recv(Libc::File_descriptor *, void *, ::size_t, int);
			int getsockopt(Libc::File_descriptor *, int, int, void *,
				       socklen_t *);
			int setsockopt(Libc::File_descriptor *, int , int , const void *,
				       socklen_t);
			int shutdown(Libc::File_descriptor *, int how);
	};


	int Plugin::stat(char const *path, struct stat *buf)
	{
		return _stat(path, buf, false);
	}


	Libc::File_descriptor *Plugin::open(char const *pathname, int flags)
	{
		if (Genode::strlen(pathname) + 1 > sizeof(sysio()->open_in.path)) {
			PDBG("ENAMETOOLONG");
			errno = ENAMETOOLONG;
			return 0;
		}

		if (flags & O_CREAT)
			unlink(pathname);

		Genode::strncpy(sysio()->open_in.path, pathname, sizeof(sysio()->open_in.path));
		sysio()->open_in.mode = flags;

		if (!noux()->syscall(Noux::Session::SYSCALL_OPEN)) {
			/*
			 * XXX  we should return meaningful errno values
			 */
			PDBG("ENOENT (sysio()->error.open=%d)", sysio()->error.open);
			errno = ENOENT;
			return 0;
		}

		Libc::Plugin_context *context = noux_context(sysio()->open_out.fd);
		return Libc::file_descriptor_allocator()->alloc(this, context, sysio()->open_out.fd);
	}


	int Plugin::fstatfs(Libc::File_descriptor *, struct statfs *buf)
	{
		buf->f_flags = MNT_UNION;
		return 0;
	}


	ssize_t Plugin::write(Libc::File_descriptor *fd, const void *buf,
	                      ::size_t count)
	{
		/* remember original len for the return value */
		int const orig_count = count;

		char *src = (char *)buf;
		while (count > 0) {

			Genode::size_t curr_count = Genode::min((::size_t)Noux::Sysio::CHUNK_SIZE, count);

			sysio()->write_in.fd = noux_fd(fd->context);
			sysio()->write_in.count = curr_count;
			Genode::memcpy(sysio()->write_in.chunk, src, curr_count);

			if (!noux()->syscall(Noux::Session::SYSCALL_WRITE)) {
				PERR("write error %d (fd %d)", sysio()->error.general, noux_fd(fd->context));
			}

			count -= curr_count;
			src   += curr_count;
		}
		return orig_count;
	}


	ssize_t Plugin::read(Libc::File_descriptor *fd, void *buf, ::size_t count)
	{
		Genode::size_t sum_read_count = 0;

		while (count) {

			Genode::size_t curr_count =
				Genode::min(count, sizeof(sysio()->read_out.chunk));

			sysio()->read_in.fd    = noux_fd(fd->context);
			sysio()->read_in.count = curr_count;

			if (!noux()->syscall(Noux::Session::SYSCALL_READ)) {
				PERR("read error");
				/* XXX set errno */
				return -1;
			}

			Genode::memcpy(buf, sysio()->read_out.chunk, sysio()->read_out.count);

			sum_read_count += sysio()->read_out.count;

			if (sysio()->read_out.count < sysio()->read_in.count)
				break; /* end of file */

			if (sysio()->read_out.count <= count)
				count -= sysio()->read_out.count;
			else
				break; /* should not happen */
		}

		return sum_read_count;
	}


	int Plugin::close(Libc::File_descriptor *fd)
	{
		sysio()->close_in.fd = noux_fd(fd->context);
		if (!noux()->syscall(Noux::Session::SYSCALL_CLOSE)) {
			PERR("close error");
			/* XXX set errno */
			return -1;
		}
		Libc::file_descriptor_allocator()->free(fd);
		return 0;
	}


	int Plugin::ioctl(Libc::File_descriptor *fd, int request, char *argp)
	{
		/*
		 * Marshal ioctl arguments
		 */
		sysio()->ioctl_in.fd = noux_fd(fd->context);
		sysio()->ioctl_in.request = Noux::Sysio::Ioctl_in::OP_UNDEFINED;

		switch (request) {

		case TIOCGWINSZ:
			sysio()->ioctl_in.request = Noux::Sysio::Ioctl_in::OP_TIOCGWINSZ;
			break;

		case TIOCGETA:
			{
				if (verbose)
					PDBG("TIOCGETA - argp=0x%p", argp);
				::termios *termios = (::termios *)argp;

				/*
				 * Set 'ECHO' flag, needed by libreadline. Otherwise, echoing
				 * user input doesn't work in bash.
				 */
				termios->c_lflag = ECHO;
				return 0;
			}

			break;

		default:

			PWRN("unsupported ioctl (request=0x%x", request);
			break;
		}

		if (sysio()->ioctl_in.request == Noux::Sysio::Ioctl_in::OP_UNDEFINED) {
			errno = ENOTTY;
			return -1;
		}

		/* perform syscall */
		if (!noux()->syscall(Noux::Session::SYSCALL_IOCTL)) {
			PERR("ioctl error");
			/* XXX set errno */
			return -1;
		}

		/*
		 * Unmarshal ioctl results
		 */
		switch (request) {

		case TIOCGWINSZ:
			{
				::winsize *winsize = (::winsize *)argp;
				winsize->ws_row = sysio()->ioctl_out.tiocgwinsz.rows;
				winsize->ws_col = sysio()->ioctl_out.tiocgwinsz.columns;
				return 0;
			}

		default:
			return -1;
		}
	}


	int Plugin::pipe(Libc::File_descriptor *pipefd[2])
	{
		/* perform syscall */
		if (!noux()->syscall(Noux::Session::SYSCALL_PIPE)) {
			PERR("pipe error");
			/* XXX set errno */
			return -1;
		}

		for (int i = 0; i < 2; i++) {
			Libc::Plugin_context *context = noux_context(sysio()->pipe_out.fd[i]);
			pipefd[i] = Libc::file_descriptor_allocator()->alloc(this, context, sysio()->pipe_out.fd[i]);
		}
		return 0;
	}


	int Plugin::dup2(Libc::File_descriptor *fd, Libc::File_descriptor *new_fd)
	{
		/*
		 * We use a one-to-one mapping of libc fds and Noux fds.
		 */
		new_fd->context = noux_context(new_fd->libc_fd);

		sysio()->dup2_in.fd    = noux_fd(fd->context);
		sysio()->dup2_in.to_fd = noux_fd(new_fd->context);

		/* perform syscall */
		if (!noux()->syscall(Noux::Session::SYSCALL_DUP2)) {
			PERR("dup2 error");
			/* XXX set errno */
			return -1;
		}

		return 0;
	}


	int Plugin::fstat(Libc::File_descriptor *fd, struct stat *buf)
	{
		sysio()->fstat_in.fd = noux_fd(fd->context);
		if (!noux()->syscall(Noux::Session::SYSCALL_FSTAT)) {
			PERR("fstat error");
			/* XXX set errno */
			return -1;
		}

		_sysio_to_stat_struct(sysio(), buf);
		return 0;
	}


	int Plugin::fsync(Libc::File_descriptor *fd)
	{
		if (verbose)
			PDBG("not implemented");
		return 0;
	}


	int Plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg)
	{
		/* copy arguments to sysio */
		sysio()->fcntl_in.fd = noux_fd(fd->context);
		switch (cmd) {

		case F_DUPFD:
			{
				/*
				 * Allocate free file descriptor locally. Noux FDs are expected
				 * to correspond one-to-one to libc FDs.
				 */
				Libc::File_descriptor *new_fd =
					Libc::file_descriptor_allocator()->alloc(this, 0);

				new_fd->context = noux_context(new_fd->libc_fd);

				/*
				 * Use new allocated number as name of file descriptor
				 * duplicate.
				 */
				if (dup2(fd, new_fd)) {
					PERR("Plugin::fcntl: dup2 unexpectedly failed");
					errno = EINVAL;
					return -1;
				}

				return new_fd->libc_fd;
			}

		case F_GETFD:
			/*
			 * Normally, we would return the file-descriptor flags.
			 *
			 * XXX: FD_CLOEXEC not yet supported
			 */
			PWRN("fcntl(F_GETFD) not implemented, returning 0");
			return 0;

		case F_SETFD:
			sysio()->fcntl_in.cmd      = Noux::Sysio::FCNTL_CMD_SET_FD_FLAGS;
			sysio()->fcntl_in.long_arg = arg;
			break;

		case F_GETFL:
			PINF("fcntl: F_GETFL for libc_fd=%d", fd->libc_fd);
			sysio()->fcntl_in.cmd = Noux::Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS;
			break;

		default:
			PERR("fcntl: unsupported command %d", cmd);
			errno = EINVAL;
			return -1;
		};

		/* invoke system call */
		if (!noux()->syscall(Noux::Session::SYSCALL_FCNTL)) {
			PWRN("fcntl failed (libc_fd= %d, cmd=%x)", fd->libc_fd, cmd);
			/* XXX read error code from sysio */
			errno = EINVAL;
			return -1;
		}

		/* read result from sysio */
		return sysio()->fcntl_out.result;
	}


	ssize_t Plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
	                              ::size_t nbytes, ::off_t *basep)
	{
		if (nbytes < sizeof(struct dirent)) {
			PERR("buf too small");
			return -1;
		}

		sysio()->dirent_in.fd = noux_fd(fd->context);

		struct dirent *dirent = (struct dirent *)buf;
		Genode::memset(dirent, 0, sizeof(struct dirent));

		if (!noux()->syscall(Noux::Session::SYSCALL_DIRENT)) {
			switch (sysio()->error.general) {

			case Noux::Sysio::ERR_FD_INVALID:
				errno = EBADF;
				PERR("dirent: ERR_FD_INVALID");
				return -1;

			case Noux::Sysio::NUM_GENERAL_ERRORS: return -1;
			}
		}

		switch (sysio()->dirent_out.entry.type) {
		case Noux::Sysio::DIRENT_TYPE_DIRECTORY: dirent->d_type = DT_DIR;  break;
		case Noux::Sysio::DIRENT_TYPE_FILE:      dirent->d_type = DT_REG;  break;
		case Noux::Sysio::DIRENT_TYPE_SYMLINK:   dirent->d_type = DT_LNK;  break;
		case Noux::Sysio::DIRENT_TYPE_FIFO:      dirent->d_type = DT_FIFO; break;
		case Noux::Sysio::DIRENT_TYPE_CHARDEV:   dirent->d_type = DT_CHR; break;
		case Noux::Sysio::DIRENT_TYPE_END:       return 0;
		}

		dirent->d_fileno = sysio()->dirent_out.entry.fileno;
		dirent->d_reclen = sizeof(struct dirent);

		Genode::strncpy(dirent->d_name, sysio()->dirent_out.entry.name,
		                sizeof(dirent->d_name));

		dirent->d_namlen = Genode::strlen(dirent->d_name);

		*basep += sizeof(struct dirent);
		return sizeof(struct dirent);
	}


	::off_t Plugin::lseek(Libc::File_descriptor *fd,
	                      ::off_t offset, int whence)
	{
		sysio()->lseek_in.fd = noux_fd(fd->context);
		sysio()->lseek_in.offset = offset;

		switch (whence) {
		default:
		case SEEK_SET: sysio()->lseek_in.whence = Noux::Sysio::LSEEK_SET; break;
		case SEEK_CUR: sysio()->lseek_in.whence = Noux::Sysio::LSEEK_CUR; break;
		case SEEK_END: sysio()->lseek_in.whence = Noux::Sysio::LSEEK_END; break;
		}

		if (!noux()->syscall(Noux::Session::SYSCALL_LSEEK)) {
			switch (sysio()->error.general) {

			case Noux::Sysio::ERR_FD_INVALID:
				errno = EBADF;
				PERR("dirent: ERR_FD_INVALID");
				return -1;

			case Noux::Sysio::NUM_GENERAL_ERRORS: return -1;
			}
		}

		return sysio()->lseek_out.offset;
	}


	int Plugin::fchdir(Libc::File_descriptor *fd)
	{
		sysio()->fchdir_in.fd = noux_fd(fd->context);
		if (!noux()->syscall(Noux::Session::SYSCALL_FCHDIR)) {
			PERR("fchdir error");
			/* XXX set errno */
			return -1;
		}

		return 0;
	}


	int Plugin::unlink(char const *path)
	{
		Genode::strncpy(sysio()->unlink_in.path, path, sizeof(sysio()->unlink_in.path));

		if (!noux()->syscall(Noux::Session::SYSCALL_UNLINK)) {
			PWRN("unlink syscall failed for path \"%s\"", path);
			switch (sysio()->error.unlink) {
			case Noux::Sysio::UNLINK_ERR_NO_ENTRY: errno = ENOENT; break;
			default:                               errno = EPERM;  break;
			}
			return -1;
		}

		return 0;
	}


	int Plugin::rename(char const *from_path, char const *to_path)
	{
		Genode::strncpy(sysio()->rename_in.from_path, from_path, sizeof(sysio()->rename_in.from_path));
		Genode::strncpy(sysio()->rename_in.to_path,   to_path,   sizeof(sysio()->rename_in.to_path));

		if (!noux()->syscall(Noux::Session::SYSCALL_RENAME)) {
			PWRN("rename syscall failed for \"%s\" -> \"%s\"", from_path, to_path);
			switch (sysio()->error.rename) {
			case Noux::Sysio::RENAME_ERR_NO_ENTRY: errno = ENOENT; break;
			case Noux::Sysio::RENAME_ERR_CROSS_FS: errno = EXDEV;  break;
			case Noux::Sysio::RENAME_ERR_NO_PERM:  errno = EPERM;  break;
			default:                               errno = EPERM;  break;
			}
			return -1;
		}

		return 0;
	}

	int Plugin::mkdir(const char *path, mode_t mode)
	{
		Genode::strncpy(sysio()->mkdir_in.path, path, sizeof(sysio()->mkdir_in.path));

		if (!noux()->syscall(Noux::Session::SYSCALL_MKDIR)) {
			PWRN("mkdir syscall failed for \"%s\" mode=0x%x", path, (int)mode);
			switch (sysio()->error.mkdir) {
			case Noux::Sysio::MKDIR_ERR_EXISTS:        errno = EEXIST;       break;
			case Noux::Sysio::MKDIR_ERR_NO_ENTRY:      errno = ENOENT;       break;
			case Noux::Sysio::MKDIR_ERR_NO_SPACE:      errno = ENOSPC;       break;
			case Noux::Sysio::MKDIR_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; break;
			case Noux::Sysio::MKDIR_ERR_NO_PERM:       errno = EPERM;        break;
			default:                                   errno = EPERM;        break;
			}
			return -1;
		}
		return 0;
	}


	Libc::File_descriptor *Plugin::socket(int domain, int type, int protocol)
	{
		sysio()->socket_in.domain = domain;
		sysio()->socket_in.type = type;
		sysio()->socket_in.protocol = protocol;

		if (!noux()->syscall(Noux::Session::SYSCALL_SOCKET))
			return 0;

		Libc::Plugin_context *context = noux_context(sysio()->socket_out.fd);
		return Libc::file_descriptor_allocator()->alloc(this, context,
				sysio()->socket_out.fd);
	}


	int Plugin::getsockopt(Libc::File_descriptor *fd, int level, int optname,
			void *optval, socklen_t *optlen)
	{
		sysio()->getsockopt_in.fd = noux_fd(fd->context);
		sysio()->getsockopt_in.level = level;
		sysio()->getsockopt_in.optname = optname;

		/* wipe-old state */
		sysio()->getsockopt_in.optlen = *optlen;
		Genode::memset(sysio()->getsockopt_in.optval, 0,
				sizeof (sysio()->getsockopt_in.optval));

		if (!noux()->syscall(Noux::Session::SYSCALL_GETSOCKOPT))
			return -1;

		Genode::memcpy(optval, sysio()->setsockopt_in.optval,
				sysio()->getsockopt_in.optlen);

		return 0;
	}


	int Plugin::setsockopt(Libc::File_descriptor *fd, int level, int optname,
			const void *optval, socklen_t optlen)
	{
		if (optlen > sizeof(sysio()->setsockopt_in.optval)) {
			/* XXX */
			return -1;
		}

		sysio()->setsockopt_in.fd = noux_fd(fd->context);
		sysio()->setsockopt_in.level = level;
		sysio()->setsockopt_in.optname = optname;
		sysio()->setsockopt_in.optlen = optlen;

		Genode::memcpy(sysio()->setsockopt_in.optval, optval, optlen);

		if (!noux()->syscall(Noux::Session::SYSCALL_SETSOCKOPT)) {
			/* XXX */
			return -1;
		}

		return 0;
	}


	Libc::File_descriptor *Plugin::accept(Libc::File_descriptor *fd, struct sockaddr *addr,
			socklen_t *addrlen)
	{
		sysio()->accept_in.fd = noux_fd(fd->context);

		if (addr != NULL) {
			Genode::memcpy(&sysio()->accept_in.addr, addr, sizeof (struct sockaddr));
			sysio()->accept_in.addrlen = *addrlen;
		}
		else {
			Genode::memset(&sysio()->accept_in.addr, 0, sizeof (struct sockaddr));
			sysio()->accept_in.addrlen = 0;
		}

		if (!noux()->syscall(Noux::Session::SYSCALL_ACCEPT)) {
			return 0;
		}

		if (addr != NULL)
			*addrlen = sysio()->accept_in.addrlen;

		Libc::Plugin_context *context = noux_context(sysio()->accept_out.fd);
		return Libc::file_descriptor_allocator()->alloc(this, context,
				sysio()->accept_out.fd);

	}


	int Plugin::bind(Libc::File_descriptor *fd, const struct sockaddr *addr,
			socklen_t addrlen)
	{
		sysio()->bind_in.fd = noux_fd(fd->context);

		Genode::memcpy(&sysio()->bind_in.addr, addr, sizeof (struct sockaddr));
		sysio()->bind_in.addrlen = addrlen;

		if (!noux()->syscall(Noux::Session::SYSCALL_BIND)) {
			errno = EACCES;
			return -1;
		}

		return 0;
	}


	int Plugin::connect(Libc::File_descriptor *fd, const struct sockaddr *addr,
			socklen_t addrlen)
	{
		sysio()->connect_in.fd = noux_fd(fd->context);

		Genode::memcpy(&sysio()->connect_in.addr, addr, sizeof (struct sockaddr));
		sysio()->connect_in.addrlen = addrlen;

		if (!noux()->syscall(Noux::Session::SYSCALL_CONNECT)) {
			/* XXX errno */
			return -1;
		}

		return 0;
	}


	void Plugin::freeaddrinfo(struct addrinfo *res)
	{
#if 0
		struct addrinfo *next;

		while (res) {
			if (res->ai_addr) {
				free(res->ai_addr);
			}

			if (res->ai_canonname) {
				free(res->ai_canonname);
			}

			next = res->ai_next;
			free(res);
			res = next;
		}
#endif
	}


	int Plugin::getaddrinfo(const char *hostname, const char *servname,
			const struct addrinfo *hints, struct addrinfo **res)
	{
#if 0
		const char *service = NULL;
		/**
		 * We have to fetch the portnumber manually because lwip only
		 * supports getting the service by portnumber. So we first check
		 * if servname is already a ascii portnumber and if it is not we
		 * call getservent(servername, NULL).
		 */
		char buf[6] = { 0 };
		int port = atoi(servname);
		if (port <= 0 || port > 0xffff) {
			struct servent *se = getservbyname(servname, NULL);
			if (se != NULL) {
				port = htons(se->s_port);
				snprintf(buf, 6, "%d", port);
				service = buf;
			}
			else {
				return -1;
			}
		}
		else
			service = servname;

		size_t len = strlen(hostname);
		len = min(len, 255);
		memcpy(sysio()->getaddrinfo_in.hostname, hostname, len);
		sysio()->getaddrinfo_in.hostname[len] = '\0';

		len = strlen(service);
		len = min(len, 255);
		memcpy(sysio()->getaddrinfo_in.servname, service, len);
		sysio()->getaddrinfo_in.servname[len] = '\0';

		if (!noux()->syscall(Noux::Session::SYSCALL_GETADDRINFO))
			return -1;

		struct addrinfo *rp = 0, *result;
		for (int i = 0; i < sysio()->getaddrinfo_out.addr_num; i++) {
			if (!rp) {
				rp = (struct addrinfo *)malloc(sizeof (struct addrinfo));
				*res = rp;
			}
			else {
				rp->ai_next = (struct addrinfo *)malloc(sizeof (struct addrinfo));
				rp = rp->ai_next;
			}

			rp->ai_flags     = sysio()->getaddrinfo_in.res[i].addrinfo.ai_flags;
			rp->ai_family    = sysio()->getaddrinfo_in.res[i].addrinfo.ai_family;
			rp->ai_socktype  = sysio()->getaddrinfo_in.res[i].addrinfo.ai_socktype;
			rp->ai_protocol  = sysio()->getaddrinfo_in.res[i].addrinfo.ai_protocol;
			rp->ai_addrlen   = sysio()->getaddrinfo_in.res[i].addrinfo.ai_addrlen;

			if (sysio()->getaddrinfo_in.res[i].ai_addr.sa_len != 0) {
				rp->ai_addr = (struct sockaddr *)malloc(sizeof (struct sockaddr));
				memcpy(rp->ai_addr, &sysio()->getaddrinfo_in.res[i].ai_addr, sizeo
						f (struct sockaddr));
			}
			else
				rp->ai_addr = 0;

			if (sysio()->getaddrinfo_in.res[i].ai_canonname != 0) {
				size_t len = strlen(sysio()->getaddrinfo_in.res[i].ai_canonname) +
					1;

				rp->ai_canonname = (char *)malloc(len);
				strncpy(rp->ai_canonname, sysio()->getaddrinfo_in.res[i].ai_canonn
						ame, len);
			}
			else
				rp->ai_canonname = 0;

			rp->ai_next = 0;
		}
#endif

		return -1;
	}


	int Plugin::getpeername(Libc::File_descriptor *fd, struct sockaddr *addr,
			socklen_t *addrlen)
	{
		sysio()->getpeername_in.fd = noux_fd(fd->context);
		sysio()->getpeername_in.addrlen = *addrlen;

		if (!noux()->syscall(Noux::Session::SYSCALL_GETPEERNAME)) {
			/* errno */
			return -1;
		}

		Genode::memcpy(addr, &sysio()->getpeername_in.addr,
		               sizeof (struct sockaddr));
		*addrlen = sysio()->getpeername_in.addrlen;

		return 0;
	}


	int Plugin::listen(Libc::File_descriptor *fd, int backlog)
	{
		sysio()->listen_in.fd = noux_fd(fd->context);
		sysio()->listen_in.backlog = backlog;

		if (!noux()->syscall(Noux::Session::SYSCALL_LISTEN)) {
			/* errno = EACCES; */
			return -1;
		}

		return 0;
	}


	ssize_t Plugin::recv(Libc::File_descriptor *fd, void *buf, ::size_t len, int flags)
	{
		Genode::size_t sum_recv_count = 0;

		while (len) {
			Genode::size_t curr_len =
				Genode::min(len, sizeof(sysio()->recv_in.buf));

			sysio()->recv_in.fd = noux_fd(fd->context);
			sysio()->recv_in.len = curr_len;

			if (!noux()->syscall(Noux::Session::SYSCALL_RECV)) {
				/* XXX set errno */
				return -1;
			}

			Genode::memcpy(buf, sysio()->recv_in.buf, sysio()->recv_in.len);

			sum_recv_count += sysio()->recv_in.len;

			if (sysio()->recv_out.len < sysio()->recv_in.len)
				break;

			if (sysio()->recv_out.len <= len)
				len -= sysio()->recv_out.len;
			else
				break;
		}

		return sum_recv_count;
	}


	ssize_t Plugin::send(Libc::File_descriptor *fd, const void *buf, ::size_t len, int flags)
	{
		/* remember original len for the return value */
		int const orig_count = len;
		char *src = (char *)buf;

		sysio()->send_in.fd = noux_fd(fd->context);
		while (len > 0) {

			Genode::size_t curr_len = Genode::min(sizeof (sysio()->send_in.buf), len);

			sysio()->send_in.len = curr_len;
			Genode::memcpy(sysio()->send_in.buf, src, curr_len);

			if (!noux()->syscall(Noux::Session::SYSCALL_SEND)) {
				PERR("write error %d", sysio()->error.general);
			}

			len -= curr_len;
			src += curr_len;
		}
		return orig_count;
	}


	ssize_t Plugin::sendto(Libc::File_descriptor *fd, const void *buf, size_t len, int flags,
			const struct sockaddr *dest_addr, socklen_t addrlen)
	{
		int const orig_count = len;

		if (addrlen > sizeof (sysio()->sendto_in.dest_addr)) {
			/* XXX errno */
			return -1;
		}

		sysio()->sendto_in.addrlen = addrlen;
		Genode::memcpy(&sysio()->sendto_in.dest_addr, dest_addr, addrlen);

		/* wipe-out sendto buffer */
		Genode::memset(sysio()->sendto_in.buf, 0, sizeof (sysio()->sendto_in.buf));

		char *src = (char *)buf;
		while (len > 0) {
			size_t curr_len = Genode::min(sizeof (sysio()->sendto_in.buf), len);

			sysio()->sendto_in.fd = noux_fd(fd->context);
			sysio()->sendto_in.len = curr_len;
			Genode::memcpy(sysio()->sendto_in.buf, src, curr_len);

			if (!noux()->syscall(Noux::Session::SYSCALL_SENDTO)) {
				return -1;
			}

			len -= curr_len;
			src += curr_len;
		}

		return orig_count;
	}


	int Plugin::shutdown(Libc::File_descriptor *fd, int how)
	{
		sysio()->shutdown_in.fd = noux_fd(fd->context);
		sysio()->shutdown_in.how = how;

		if (!noux()->syscall(Noux::Session::SYSCALL_SHUTDOWN)) {
			return -1;
		}

		return 0;
	}

} /* unnamed namespace */


/**************************************
 ** Obtaining command-line arguments **
 **************************************/

/* external symbols provided by Genode's startup code */
extern char **genode_argv;
extern int    genode_argc;
extern char **genode_envp;

/* pointer to environment, provided by libc */
extern char **environ;

__attribute__((constructor))
void init_libc_noux(void)
{
	/* copy command-line arguments from 'args' ROM dataspace */
	Genode::Rom_connection args_rom("args");
	char *args = (char *)Genode::env()->rm_session()->
		attach(args_rom.dataspace());

	enum { MAX_ARGS = 256, ARG_BUF_SIZE = 4096 };
	static char *argv[MAX_ARGS];
	static char  arg_buf[ARG_BUF_SIZE];
	Genode::memcpy(arg_buf, args, ARG_BUF_SIZE);

	int argc = 0;
	for (unsigned i = 0; arg_buf[i] && (i < ARG_BUF_SIZE - 2); ) {

		if (i >= ARG_BUF_SIZE - 2) {
			PWRN("command-line argument buffer exceeded\n");
			break;
		}

		if (argc >= MAX_ARGS - 1) {
			PWRN("number of command-line arguments exceeded\n");
			break;
		}

		argv[argc] = &arg_buf[i];
		i += Genode::strlen(&args[i]) + 1; /* skip null-termination */
		argc++;
	}

	/* register command-line arguments at Genode's startup code */
	genode_argv = argv;
	genode_argc = argc;

	/*
	 * Make environment variables from 'env' ROM dataspace available to libc's
	 * 'environ'.
	 */
	Genode::Rom_connection env_rom("env");
	Genode::Dataspace_capability env_ds = env_rom.dataspace();
	char *env_string = (char *)Genode::env()->rm_session()->attach(env_ds);

	enum { ENV_MAX_SIZE       = 4096,
	       ENV_MAX_ENTRIES    =  128,
	       ENV_KEY_MAX_SIZE   =  256,
	       ENV_VALUE_MAX_SIZE = 1024 };

	static char  env_buf[ENV_MAX_SIZE];
	static char *env_array[ENV_MAX_ENTRIES];

	unsigned num_entries = 0;  /* index within 'env_array' */
	Genode::size_t i     = 0;  /* index within 'env_buf' */

	while ((num_entries < ENV_MAX_ENTRIES - 2) && (i < ENV_MAX_SIZE)) {

		Genode::Arg arg = Genode::Arg_string::first_arg(env_string);
		if (!arg.valid())
			break;

		char key_buf[ENV_KEY_MAX_SIZE],
		     value_buf[ENV_VALUE_MAX_SIZE];

		arg.key(key_buf, sizeof(key_buf));
		arg.string(value_buf, sizeof(value_buf), "");

		env_array[num_entries++] = env_buf + i;
		Genode::snprintf(env_buf + i, ENV_MAX_SIZE - i,
		                 "%s=%s", key_buf, value_buf);

		i += Genode::strlen(env_buf + i) + 1;

		/* remove processed arg from 'env_string' */
		Genode::Arg_string::remove_arg(env_string, key_buf);
	}

	/* register list of environment variables at libc 'environ' pointer */
	environ = env_array;

	/* define env pointer to be passed to main function (in '_main.cc') */
	genode_envp = environ;

	/* initialize noux libc plugin */
	static Plugin noux_plugin;
}


