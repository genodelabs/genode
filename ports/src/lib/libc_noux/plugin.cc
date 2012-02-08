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
#include <unistd.h>
#include <termios.h>



Noux::Session *noux()
{
	static Noux::Connection noux_inst;
	return &noux_inst;
}


Noux::Sysio *sysio()
{
	using namespace Noux;

	static Sysio *sysio =
		Genode::env()->rm_session()->attach(noux()->sysio_dataspace());

	return sysio;
}

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


static int _stat(const char *path, struct stat *buf, bool lstat = false)
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


extern "C" int lstat(const char *path, struct stat *buf) { return _stat(path, buf, true);  }


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


extern "C" int execve(const char *filename, char *const argv[],
                      char *const envp[])
{
	PDBG("filename=%s", filename);

	for (int i = 0; argv[i]; i++)
		PDBG("argv[%d]='%s'", i, argv[i]);

	for (int i = 0; envp[i]; i++)
		PDBG("envp[%d]='%s'", i, envp[i]);

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

			bool supports_chdir(const char *)     { return true; }
			bool supports_open(const char *, int) { return true; }
			bool supports_stat(const char *)      { return true; }

			Libc::File_descriptor *open(const char *, int);
			ssize_t write(Libc::File_descriptor *, const void *, ::size_t);
			int close(Libc::File_descriptor *);
			int fstat(Libc::File_descriptor *, struct stat *);
			int fstatfs(Libc::File_descriptor *, struct statfs *);
			int fcntl(Libc::File_descriptor *, int, long);
			ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t, ::off_t *);
			::off_t lseek(Libc::File_descriptor *, ::off_t offset, int whence);
			int fchdir(Libc::File_descriptor *);
			ssize_t read(Libc::File_descriptor *, void *, ::size_t);
			int stat(const char *, struct stat *);
			int ioctl(Libc::File_descriptor *, int request, char *argp);
	};


	int Plugin::stat(const char *path, struct stat *buf)
	{
		return _stat(path, buf, false);
	}


	Libc::File_descriptor *Plugin::open(const char *pathname, int flags)
	{
		if (Genode::strlen(pathname) + 1 > sizeof(sysio()->open_in.path)) {
			errno = ENAMETOOLONG;
			return 0;
		}

		Genode::strncpy(sysio()->open_in.path, pathname, sizeof(sysio()->open_in.path));
		sysio()->open_in.mode = flags;

		if (!noux()->syscall(Noux::Session::SYSCALL_OPEN)) {
			/*
			 * XXX  we should return meaningful errno values
			 */
			errno = ENOENT;
			return 0;
		}

		Libc::Plugin_context *context = noux_context(sysio()->open_out.fd);
		return Libc::file_descriptor_allocator()->alloc(this, context);
	}


	int Plugin::fstatfs(Libc::File_descriptor *, struct statfs *buf)
	{
		return 0;
	}


	ssize_t Plugin::write(Libc::File_descriptor *fd, const void *buf,
	                      ::size_t count)
	{
		if (fd != _stdout && fd != _stderr) {
			errno = EBADF;
			return -1;
		}

		/* remember original len for the return value */
		int const orig_count = count;

		char *src = (char *)buf;
		while (count > 0) {

			Genode::size_t curr_count = Genode::min((::size_t)Noux::Sysio::CHUNK_SIZE, count);

			sysio()->write_in.fd = noux_fd(fd->context);
			sysio()->write_in.count = curr_count;
			Genode::memcpy(sysio()->write_in.chunk, src, curr_count);

			if (!noux()->syscall(Noux::Session::SYSCALL_WRITE)) {
				PERR("write error %d", sysio()->error.general);
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

//			for (int i = 0; i < sysio()->read_out.count; i++)
//				Genode::printf("read %d\n", ((char *)buf)[i]);

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


	int Plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg)
	{
		/* copy arguments to sysio */
		sysio()->fcntl_in.fd = noux_fd(fd->context);
		switch (cmd) {

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

		unsigned const curr_offset = *basep;

		sysio()->dirent_in.fd = noux_fd(fd->context);
		sysio()->dirent_in.index = curr_offset / sizeof(struct dirent);

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
		PWRN("lseek - not implemented: fd=%d, offset=%ld, whence=%d",
		     noux_fd(fd->context), (long)offset, whence);

		if (whence == SEEK_SET)
			return offset;

		return 0;
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

} /* unnamed namespace */


/**************************************
 ** Obtaining command-line arguments **
 **************************************/

/* external symbols provided by Genode's startup code */
extern char **genode_argv;
extern int    genode_argc;

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

	/* initialize noux libc plugin */
	static Plugin noux_plugin;
}


