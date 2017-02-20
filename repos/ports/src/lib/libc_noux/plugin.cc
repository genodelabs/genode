/*
 * \brief  Noux libc plugin
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
#include <util/construct_at.h>
#include <util/misc_math.h>
#include <util/arg_string.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <region_map/client.h>

/* noux includes */
#include <noux_session/connection.h>
#include <noux_session/sysio.h>

/* libc plugin includes */
#include <libc-plugin/plugin.h>
#include <libc-plugin/fd_alloc.h>

/* libc includes */
#include <errno.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>
#include <string.h>
#include <signal.h>

/**
 * There is a off_t typedef clash between sys/socket.h
 * and base/stdint.h. We define the macro here to circumvent
 * this issue.
 */
#undef DIOCGMEDIASIZE
#define DIOCGMEDIASIZE _IOR('d', 129, int64_t)


/* libc-internal includes */
#include <libc_mem_alloc.h>


using Genode::log;
using Genode::error;
using Genode::warning;
using Genode::Hex;


enum { verbose = false };
enum { verbose_signals = false };


/*
 * Customize libc VFS
 */
namespace Libc {

	/*
	 * Override the weak function interface of the libc and VFS plugin as Noux
	 * programs do not obtain such configuration via Genode's config mechanism.
	 */
	Genode::Xml_node config()     { return Xml_node("<libc/>"); }
	Genode::Xml_node vfs_config() { return Xml_node("<vfs/>");  }
}



class Noux_connection
{
	private:

		Noux::Connection _connection;

		Genode::Attached_dataspace _sysio_ds { _connection.sysio_dataspace() };

	public:

		/**
		 * Return the capability of the local stack-area region map
		 *
		 * \param ptr  some address within the stack-area
		 */
		Genode::Capability<Genode::Region_map> stack_area_region_map(void * const ptr)
		{
			return _connection.lookup_region_map((Genode::addr_t)ptr);
		}

		Noux::Session *session() { return &_connection; }
		Noux::Sysio   *sysio()   { return _sysio_ds.local_addr<Noux::Sysio>(); }

		void reconnect()
		{
			using namespace Genode;

			/*
			 * Release Id_space<Parent::Client>::Element of the local ID space.
			 */
			_connection.discard_session_id();

			/*
			 * Obtain new noux connection. Note that we cannot reconstruct
			 * the connection via a 'Reconstructible' because this would
			 * result in an inconsistent referernce count when attempting
			 * to destruct the session capability in the just-cleared
			 * capability space.
			 */
			construct_at<Noux_connection>(this);
		}
};


Noux_connection *noux_connection()
{
	static Noux_connection inst;
	return &inst;
}


Noux::Session *noux()  { return noux_connection()->session(); }
Noux::Sysio   *sysio() { return noux_connection()->sysio();   }


/*
 * Array of signal handlers, initialized with 0 (SIG_DFL)
 * TODO: preserve ignored signals across 'execve()'
 */
static struct sigaction signal_action[NSIG+1];

/*
 * Signal mask functionality is not fully implemented yet.
 * TODO: - actually block delivery of to be blocked signals
 *       - preserve signal mask across 'execve()'
 */
static sigset_t signal_mask;


static bool noux_syscall(Noux::Session::Syscall opcode)
{
	/*
	 * Signal handlers might do syscalls themselves, so the 'sysio' object
	 * needs to be saved before and restored after calling the signal handler.
	 * There is only one global 'saved_sysio' buffer as signals are not checked
	 * in nested calls of 'noux_syscall' from signal handlers.
	 */
	static Noux::Sysio saved_sysio;

	bool ret = noux()->syscall(opcode);

	static bool in_sigh = false; /* true if called from signal handler */

	if (in_sigh)
		return ret;

	/* handle signals */
	while (!sysio()->pending_signals.empty()) {
		in_sigh = true;
		Noux::Sysio::Signal signal = sysio()->pending_signals.get();
		if (verbose_signals)
			log(__func__, ": received signal ", (int)signal);
		if (signal_action[signal].sa_flags & SA_SIGINFO) {
			memcpy(&saved_sysio, sysio(), sizeof(Noux::Sysio));
			/* TODO: pass siginfo_t struct */
			signal_action[signal].sa_sigaction(signal, 0, 0);
			memcpy(sysio(), &saved_sysio, sizeof(Noux::Sysio));
		} else {
			if (signal_action[signal].sa_handler == SIG_DFL) {
				switch (signal) {
					case SIGCHLD:
						/* ignore */
						break;
					default:
						/* terminate the process */
						exit((signal << 8) | EXIT_FAILURE);
				}
			} else if (signal_action[signal].sa_handler == SIG_IGN) {
				/* do nothing */
			} else {
				memcpy(&saved_sysio, sysio(), sizeof(Noux::Sysio));
				signal_action[signal].sa_handler(signal);
				memcpy(sysio(), &saved_sysio, sizeof(Noux::Sysio));
			}
		}
	}
	in_sigh = false;

	return ret;
}


enum { FS_BLOCK_SIZE = 1024 };


/***********************************************
 ** Overrides of libc default implementations **
 ***********************************************/

/**
 * User information related functions
 */
extern "C" struct passwd *getpwuid(uid_t uid)
{
	static char name[Noux::Sysio::MAX_USERNAME_LEN];
	static char shell[Noux::Sysio::MAX_SHELL_LEN];
	static char home[Noux::Sysio::MAX_HOME_LEN];

	static char *empty = strdup("");

	static struct passwd pw = {
		/* .pw_name    = */ name,
		/* .pw_passwd  = */ empty,
		/* .pw_uid     = */ 0,
		/* .pw_gid     = */ 0,
		/* .pw_change  = */ 0,
		/* .pw_class   = */ empty,
		/* .pw_gecos   = */ empty,
		/* .pw_dir     = */ home,
		/* .pw_shell   = */ shell,
		/* .pw_expire  = */ 0,
		/* .pw_fields  = */ 0
	};

	sysio()->userinfo_in.uid = uid;
	sysio()->userinfo_in.request = Noux::Sysio::USERINFO_GET_ALL;

	if (!noux_syscall(Noux::Session::SYSCALL_USERINFO)) {
		return (struct passwd *)0;
	}

	/* SYSCALL_USERINFO assures that strings are always '\0' terminated */
	Genode::memcpy(name,  sysio()->userinfo_out.name,  sizeof (sysio()->userinfo_out.name));
	Genode::memcpy(home,  sysio()->userinfo_out.home,  sizeof (sysio()->userinfo_out.home));
	Genode::memcpy(shell, sysio()->userinfo_out.shell, sizeof (sysio()->userinfo_out.shell));

	pw.pw_uid = sysio()->userinfo_out.uid;
	pw.pw_gid = sysio()->userinfo_out.gid;

	return &pw;
}


extern "C" int getdtablesize()
{
	if (!noux_syscall(Noux::Session::SYSCALL_GETDTABLESIZE)) {
		warning("getdtablesize syscall failed");
		errno = ENOSYS;
		return -1;
	}

	int n = sysio()->getdtablesize_out.n;
	return n;
}


extern "C" uid_t getgid()
{
	sysio()->userinfo_in.request = Noux::Sysio::USERINFO_GET_GID;

	if (!noux_syscall(Noux::Session::SYSCALL_USERINFO))
		return 0;

	uid_t gid = sysio()->userinfo_out.gid;
	return gid;
}


extern "C" uid_t getegid()
{
	return getgid();
}


extern "C" uid_t getuid()
{
	sysio()->userinfo_in.request = Noux::Sysio::USERINFO_GET_UID;

	if (!noux_syscall(Noux::Session::SYSCALL_USERINFO))
		return 0;

	uid_t uid = sysio()->userinfo_out.uid;

	return uid;
}


extern "C" uid_t geteuid()
{
	return getuid();
}


void *sbrk(intptr_t increment)
{
	if (verbose)
		warning(__func__, " not implemented ", (long int)increment);
	errno = ENOMEM;
	return reinterpret_cast<void *>(-1);
}


extern "C" int getrlimit(int resource, struct rlimit *rlim)
{
	switch (resource) {
		case RLIMIT_STACK:
		{
			using namespace Genode;

			Thread * me = Thread::myself();

			if (!me)
				break;

			addr_t top = reinterpret_cast<addr_t>(me->stack_top());
			addr_t cur = reinterpret_cast<addr_t>(me->stack_base());

			rlim->rlim_cur = rlim->rlim_max = top - cur;
			return 0;
		}
		case RLIMIT_AS:
			#ifdef __x86_64__
				rlim->rlim_cur = rlim->rlim_max = 0x800000000000UL;
			#else
				rlim->rlim_cur = rlim->rlim_max = 0xc0000000UL;
			#endif
			return 0;
		case RLIMIT_RSS:
			rlim->rlim_cur = rlim->rlim_max = Genode::env()->ram_session()->quota();
			return 0;
		case RLIMIT_NPROC:
		case RLIMIT_NOFILE:
			rlim->rlim_cur = rlim->rlim_max = RLIM_INFINITY;
			return 0;
	}
	errno = ENOSYS;
	warning(__func__, " not implemented (resource=", resource, ")");
	return -1;
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
static void unmarshal_fds(int nfds, int *src_fds, size_t src_fds_len, fd_set *dst_fds)
{
	if (!dst_fds) return;

	/**
	 * Calling FD_ZERO will not work because it will try to reset sizeof (fd_set)
	 * which is typically 128 bytes but dst_fds might by even less bytes large if
	 * it was allocated dynamically. So we will reset the fd_set manually which
	 * will work fine as long as we are using FreeBSDs libc - another libc however
	 * might use a different struct.
	 *
	 * Note: The fds are actually stored in a bit-array. So we need to calculate
	 * how many array entries we have to reset. sizeof (fd_mask) will return the
	 * size of one entry in bytes.
	 */
	int _ = nfds / (sizeof (fd_mask) * 8) + 1;
	for (int i = 0; i < _; i++)
		dst_fds->__fds_bits[i] = 0;

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

	/**
	 * These variables are used in max_fds_exceeded() calculation, so
	 * they need to be proper initialized.
	 */
	in_fds.num_rd = 0;
	in_fds.num_wr = 0;
	in_fds.num_ex = 0;

	if (readfds != NULL) {
		in_fds.num_rd = marshal_fds(readfds, nfds, dst, dst_len);

		dst     += in_fds.num_rd;
		dst_len -= in_fds.num_rd;
	}

	if (writefds != NULL) {
		in_fds.num_wr = marshal_fds(writefds, nfds, dst, dst_len);

		dst     += in_fds.num_wr;
		dst_len -= in_fds.num_wr;
	}

	if (exceptfds != NULL) {
		in_fds.num_ex = marshal_fds(exceptfds, nfds, dst, dst_len);
	}

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
	} else {
		sysio()->select_in.timeout.set_infinite();
	}

	/*
	 * Perform syscall
	 */
	if (!noux_syscall(Noux::Session::SYSCALL_SELECT)) {
		switch (sysio()->error.select) {
			case Noux::Sysio::SELECT_ERR_INTERRUPT: errno = EINTR; break;
		}
		return -1;
	}

	/*
	 * Unmarshal file selectors reported by the select syscall
	 */
	Noux::Sysio::Select_fds &out_fds = sysio()->select_out.fds;

	int *src = out_fds.array;
	int total_fds = 0;

	if (readfds != NULL) {
		unmarshal_fds(nfds, src, out_fds.num_rd, readfds);
		src += out_fds.num_rd;
		total_fds += out_fds.num_rd;
	}

	if (writefds != NULL) {
		unmarshal_fds(nfds, src, out_fds.num_wr, writefds);
		src += out_fds.num_wr;
		total_fds += out_fds.num_wr;
	}

	if (exceptfds != NULL) {
		unmarshal_fds(nfds, src, out_fds.num_ex, exceptfds);
		/* exceptfds are currently ignored */
	}

	return total_fds;
}


#include <setjmp.h>


static void * in_stack_area;
static jmp_buf fork_jmp_buf;
static Genode::Capability<Genode::Parent>::Raw new_parent;

extern "C" void stdout_reconnect(); /* provided by 'log_console.cc' */


/*
 * The new process created via fork will start its execution here.
 */
extern "C" void fork_trampoline()
{
	/* reinitialize environment */
	using namespace Genode;
	env()->reinit(new_parent);

	/* reinitialize standard-output connection */
	stdout_reconnect();

	/* reinitialize noux connection */
	noux_connection()->reconnect();

	/* reinitialize main-thread object which implies reinit of stack area */
	auto stack_area_rm = noux_connection()->stack_area_region_map(in_stack_area);
	Genode::env()->reinit_main_thread(stack_area_rm);

	/* apply processor state that the forker had when he did the fork */
	longjmp(fork_jmp_buf, 1);
}


static pid_t fork_result;


/**
 * Called once the component has left the entrypoint and exited the signal
 * dispatch loop.
 *
 * This function is called from the context of the initial thread.
 */
static void suspended_callback()
{
	/* stack used for executing 'fork_trampoline' */
	enum { STACK_SIZE = 8 * 1024 };
	static long stack[STACK_SIZE];

	if (setjmp(fork_jmp_buf)) {

		/*
		 * We got here via longjmp from 'fork_trampoline'.
		 */
		fork_result = 0;

	} else {

		/*
		 * save the current stack address used for re-initializing
		 * the stack area during process bootstrap
		 */
		int dummy;
		in_stack_area = &dummy;

		/* got here during the normal control flow of the fork call */
		sysio()->fork_in.ip = (Genode::addr_t)(&fork_trampoline);
		sysio()->fork_in.sp = Abi::stack_align((Genode::addr_t)&stack[STACK_SIZE]);
		sysio()->fork_in.parent_cap_addr = (Genode::addr_t)(&new_parent);

		if (!noux_syscall(Noux::Session::SYSCALL_FORK)) {
			error("fork error ", (int)sysio()->error.general);
			switch (sysio()->error.fork) {
			case Noux::Sysio::FORK_NOMEM: errno = ENOMEM; break;
			default: errno = EAGAIN;
			}
			fork_result = -1;
			return;
		}

		fork_result = sysio()->fork_out.pid;
	}
}


namespace Libc { void schedule_suspend(void (*suspended) ()); }


extern "C" pid_t fork(void)
{
	Libc::schedule_suspend(suspended_callback);

	return fork_result;
}


extern "C" pid_t vfork(void) { return fork(); }


extern "C" pid_t getpid(void)
{
	noux_syscall(Noux::Session::SYSCALL_GETPID);
	return sysio()->getpid_out.pid;
}


extern "C" pid_t getppid(void) { return getpid(); }


extern "C" int chmod(char const *path, mode_t mode)
{
	if (verbose)
		warning(__func__, ": chmod '", path, "' to ", Hex(mode), " not implemented");
	return 0;
}


extern "C" pid_t _wait4(pid_t pid, int *status, int options,
                        struct rusage *rusage)
{
	sysio()->wait4_in.pid    = pid;
	sysio()->wait4_in.nohang = !!(options & WNOHANG);
	if (!noux_syscall(Noux::Session::SYSCALL_WAIT4)) {
		switch (sysio()->error.wait4) {
			case Noux::Sysio::WAIT4_ERR_INTERRUPT: errno = EINTR; break;
		}
		return -1;
	}

	/*
	 * The libc expects status information in bits 0..6 and the exit value
	 * in bits 8..15 (according to 'wait.h'). 
	 */
	if (status)
		*status = ((sysio()->wait4_out.status >> 8) & 0177) |
		          ((sysio()->wait4_out.status & 0xff) << 8);

	return sysio()->wait4_out.pid;
}


int getrusage(int who, struct rusage *usage)
{
	if (verbose)
		warning(__func__, " not implemented");

	errno = ENOSYS;
	return -1;
}


void endpwent(void)
{
	if (verbose)
		warning(__func__, " not implemented");
}


extern "C" void sync(void)
{
	noux_syscall(Noux::Session::SYSCALL_SYNC);
}


extern "C" int kill(int pid, int sig)
{
	if (verbose_signals)
		log(__func__, ": pid=", pid, ", sig=", sig);

	sysio()->kill_in.pid = pid;
	sysio()->kill_in.sig = Noux::Sysio::Signal(sig);

	if (!noux_syscall(Noux::Session::SYSCALL_KILL)) {
		switch (sysio()->error.kill) {
			case Noux::Sysio::KILL_ERR_SRCH: errno = ESRCH; break;
		}
		return -1;
	}

	return 0;
}


extern "C" int nanosleep(const struct timespec *timeout,
                         struct timespec *remainder)
{
	Noux::Sysio::Select_fds &in_fds = sysio()->select_in.fds;

	in_fds.num_rd = 0;
	in_fds.num_wr = 0;
	in_fds.num_ex = 0;

	sysio()->select_in.timeout.sec  = timeout->tv_sec;
	sysio()->select_in.timeout.usec = timeout->tv_nsec / 1000;

	/*
	 * Perform syscall
	 */
	if (!noux_syscall(Noux::Session::SYSCALL_SELECT)) {
		switch (sysio()->error.select) {
		case Noux::Sysio::SELECT_ERR_INTERRUPT: errno = EINTR; break;
		}

		return -1;
	}

	if (remainder) {
		remainder->tv_sec  = 0;
		remainder->tv_nsec = 0;
	}

	return 0;
}


extern "C" unsigned int sleep(unsigned int seconds)
{
	struct timespec dummy = { Genode::min<int>(seconds, __INT_MAX__), 0 };

	/*
	 * Always return 0 because our nanosleep() cannot not be interrupted.
	 */
	nanosleep(&dummy, 0);
	return 0;
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
	/* we currently only support CLOCK_SECOND */
	switch (clk_id) {
	case CLOCK_SECOND:
		sysio()->clock_gettime_in.clock_id = Noux::Sysio::CLOCK_ID_SECOND;
		break;
	default:
		/* let's save the trip to noux and return directly */
		errno = EINVAL;
		return -1;
	}

	if (!noux_syscall(Noux::Session::SYSCALL_CLOCK_GETTIME)) {
		switch (sysio()->error.clock) {
		case Noux::Sysio::CLOCK_ERR_INVALID: errno = EINVAL; break;
		default:                             errno = 0;      break;
		}

		return -1;
	}

	tp->tv_sec = sysio()->clock_gettime_out.sec;
	tp->tv_nsec = sysio()->clock_gettime_out.nsec;

	return 0;
}


extern "C" int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (!noux_syscall(Noux::Session::SYSCALL_GETTIMEOFDAY)) {
		errno = EINVAL;
		return -1;
	}

	tv->tv_sec  = sysio()->gettimeofday_out.sec;
	tv->tv_usec = sysio()->gettimeofday_out.usec;

	return 0;
}


extern "C" int utimes(const char* path, const struct timeval *times)
{
	if (!noux_syscall(Noux::Session::SYSCALL_UTIMES)) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}


/*********************
 ** Signal handling **
 *********************/

extern "C" int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	if (oldset)
		*oldset = signal_mask;

	if (!set)
		return 0;

	switch (how) {
		case SIG_BLOCK:
			for (int sig = 1; sig < NSIG; sig++)
				if (sigismember(set, sig)) {
					if (verbose_signals)
						log(__func__, ": signal ", sig, " requested to get blocked");
					sigaddset(&signal_mask, sig);
				}
			break;
		case SIG_UNBLOCK:
			for (int sig = 1; sig < NSIG; sig++)
				if (sigismember(set, sig)) {
					if (verbose_signals)
						log(__func__, ": signal ", sig, " requested to get unblocked");
					sigdelset(&signal_mask, sig);
				}
			break;
		case SIG_SETMASK:
			if (verbose_signals)
				for (int sig = 1; sig < NSIG; sig++)
					if (sigismember(set, sig))
						log(__func__, ": signal ", sig, " requested to get blocked");
			signal_mask = *set;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	return 0;
}


extern "C" int _sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	return sigprocmask(how, set, oldset);
}


extern "C" int _sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if (verbose_signals)
		log("signum=", signum, ", handler=", act ? act->sa_handler : nullptr);

	if ((signum < 1) || (signum > NSIG)) {
		errno = EINVAL;
		return -1;
	}

	if (oldact)
		*oldact = signal_action[signum];

	if (act)
		signal_action[signum] = *act;

	return 0;
}


extern "C" int sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact)
{
	return _sigaction(signum, act, oldact);
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

			/*
			 * Override the libc's default VFS plugin
			 */
			enum { PLUGIN_PRIORITY = 1 };

		public:

			/**
			 * Constructor
			 */
			Plugin() : Libc::Plugin(PLUGIN_PRIORITY)
			{
				/* register inherited open file descriptors */
				int fd = 0;
				while ((fd = noux()->next_open_fd(fd)) != -1) {
					Libc::file_descriptor_allocator()->alloc(this, noux_context(fd), fd);
					fd++;
				}
			}

			bool supports_access(const char *, int)                { return true; }
			bool supports_execve(char const *, char *const[],
			                     char *const[])                    { return true; }
			bool supports_open(char const *, int)                  { return true; }
			bool supports_stat(char const *)                       { return true; }
			bool supports_symlink(char const *, char const*)       { return true; }
			bool supports_pipe()                                   { return true; }
			bool supports_unlink(char const *)                     { return true; }
			bool supports_readlink(const char *, char *, ::size_t) { return true; }
			bool supports_rename(const char *, const char *)       { return true; }
			bool supports_rmdir(char const *)                      { return true; }
			bool supports_mkdir(const char *, mode_t)              { return true; }
			bool supports_socket(int, int, int)                    { return true; }
			bool supports_mmap()                                   { return true; }

			int access(char const *, int);
			Libc::File_descriptor *open(char const *, int);
			ssize_t write(Libc::File_descriptor *, const void *, ::size_t);
			int close(Libc::File_descriptor *);
			Libc::File_descriptor *dup(Libc::File_descriptor*);
			int dup2(Libc::File_descriptor *, Libc::File_descriptor *);
			int execve(char const *filename, char *const argv[],
			           char *const envp[]);
			int fstat(Libc::File_descriptor *, struct stat *);
			int fsync(Libc::File_descriptor *);
			int fstatfs(Libc::File_descriptor *, struct statfs *);
			int ftruncate(Libc::File_descriptor *, ::off_t);
			int fcntl(Libc::File_descriptor *, int, long);
			ssize_t getdirentries(Libc::File_descriptor *, char *, ::size_t, ::off_t *);
			::off_t lseek(Libc::File_descriptor *, ::off_t offset, int whence);
			ssize_t read(Libc::File_descriptor *, void *, ::size_t);
			ssize_t readlink(const char *path, char *buf, ::size_t bufsiz);
			int rename(const char *oldpath, const char *newpath);
			int rmdir(char const *path);
			int stat(char const *, struct stat *);
			int symlink(const char *, const char *);
			int ioctl(Libc::File_descriptor *, int request, char *argp);
			int pipe(Libc::File_descriptor *pipefd[2]);
			int unlink(char const *path);
			int mkdir(const char *path, mode_t mode);
			void *mmap(void *addr, ::size_t length, int prot, int flags,
			           Libc::File_descriptor *, ::off_t offset);
			int munmap(void *addr, ::size_t length);

			/* Network related functions */
			Libc::File_descriptor *socket(int, int, int);
			Libc::File_descriptor *accept(Libc::File_descriptor *,
						      struct sockaddr *, socklen_t *);
			int bind(Libc::File_descriptor *, const struct sockaddr *,
				 socklen_t);
			int connect(Libc::File_descriptor *, const struct sockaddr *addr,
				    socklen_t addrlen);
			int getpeername(Libc::File_descriptor *, struct sockaddr *,
					socklen_t *);
			int listen(Libc::File_descriptor *, int);
			ssize_t send(Libc::File_descriptor *, const void *, ::size_t,
				     int flags);
			ssize_t sendto(Libc::File_descriptor *, const void *, size_t, int,
				       const struct sockaddr *, socklen_t);
			ssize_t recv(Libc::File_descriptor *, void *, ::size_t, int);
			ssize_t recvfrom(Libc::File_descriptor *, void *, ::size_t, int,
			                 struct sockaddr *, socklen_t*);
			int getsockopt(Libc::File_descriptor *, int, int, void *,
				       socklen_t *);
			int setsockopt(Libc::File_descriptor *, int , int , const void *,
				       socklen_t);
			int shutdown(Libc::File_descriptor *, int how);
	};


	int Plugin::access(char const *pathname, int mode)
	{
		if (verbose)
			log(__func__, ": access '", pathname, "' (mode=", Hex(mode), ") "
			    "called, not implemented");

		struct stat stat;
		if (::stat(pathname, &stat) == 0)
			return 0;

		errno = ENOENT;
		return -1;
	}


	int Plugin::execve(char const *filename, char *const argv[],
	                   char *const envp[])
	{
		if (verbose) {
			log(__func__, ": filename=", filename);

			for (int i = 0; argv[i]; i++)
				log(__func__, "argv[", i, "]='", Genode::Cstring(argv[i]), "'");

			for (int i = 0; envp[i]; i++)
				log(__func__, "envp[", i, "]='", Genode::Cstring(envp[i]), "'");
		}

		Genode::strncpy(sysio()->execve_in.filename, filename, sizeof(sysio()->execve_in.filename));
		if (!serialize_string_array(argv, sysio()->execve_in.args,
		                            sizeof(sysio()->execve_in.args))) {
		    error("execve: argument buffer exceeded");
		    errno = E2BIG;
		    return -1;
		}

		/* communicate the current working directory as environment variable */

		size_t noux_cwd_len = Genode::snprintf(sysio()->execve_in.env,
		                                       sizeof(sysio()->execve_in.env),
		                                       "NOUX_CWD=");

		if (!getcwd(&(sysio()->execve_in.env[noux_cwd_len]),
		            sizeof(sysio()->execve_in.env) - noux_cwd_len)) {
		    error("execve: environment buffer exceeded");
		    errno = E2BIG;
		    return -1;
		}

		noux_cwd_len = strlen(sysio()->execve_in.env) + 1;

		if (!serialize_string_array(envp, &(sysio()->execve_in.env[noux_cwd_len]),
                                   sizeof(sysio()->execve_in.env) - noux_cwd_len)) {
		    error("execve: environment buffer exceeded");
		    errno = E2BIG;
		    return -1;
		}

		if (!noux_syscall(Noux::Session::SYSCALL_EXECVE)) {
			warning("exec syscall failed for path \"", filename, "\"");
			switch (sysio()->error.execve) {
			case Noux::Sysio::EXECVE_NONEXISTENT: errno = ENOENT; break;
			case Noux::Sysio::EXECVE_NOMEM:       errno = ENOMEM; break;
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


	int Plugin::stat(char const *path, struct stat *buf)
	{
		if (verbose)
			log(__func__, ": path=", path);

		if ((path == NULL) or (buf == NULL)) {
			errno = EFAULT;
			return -1;
		}

		Genode::strncpy(sysio()->stat_in.path, path, sizeof(sysio()->stat_in.path));

		if (!noux_syscall(Noux::Session::SYSCALL_STAT)) {
			if (verbose)
				warning("stat syscall failed for path \"", path, "\"");
			switch (sysio()->error.stat) {
			case Vfs::Directory_service::STAT_ERR_NO_ENTRY: errno = ENOENT; return -1;
			case Vfs::Directory_service::STAT_ERR_NO_PERM:  errno = EACCES; return -1;
			case Vfs::Directory_service::STAT_OK: break; /* never reached */
			}
		}

		_sysio_to_stat_struct(sysio(), buf);
		return 0;
	}


	Libc::File_descriptor *Plugin::open(char const *pathname, int flags)
	{
		if (Genode::strlen(pathname) + 1 > sizeof(sysio()->open_in.path)) {
			log(__func__, ": ENAMETOOLONG");
			errno = ENAMETOOLONG;
			return 0;
		}

		bool opened = false;
		while (!opened) {
			Genode::strncpy(sysio()->open_in.path, pathname, sizeof(sysio()->open_in.path));
			sysio()->open_in.mode = flags;
			if (noux_syscall(Noux::Session::SYSCALL_OPEN))
				opened = true;
			else
				switch (sysio()->error.open) {
				case Vfs::Directory_service::OPEN_ERR_UNACCESSIBLE:
						if (!(flags & O_CREAT)) {
							errno = ENOENT;
							return 0;
						}
						/* O_CREAT is set, so try to create the file */
						Genode::strncpy(sysio()->open_in.path, pathname, sizeof(sysio()->open_in.path));
						sysio()->open_in.mode = flags | O_EXCL;
						if (noux_syscall(Noux::Session::SYSCALL_OPEN))
							opened = true;
						else
							switch (sysio()->error.open) {
							case Vfs::Directory_service::OPEN_ERR_EXISTS:
								/* file has been created by someone else in the meantime */
								break;
							case Vfs::Directory_service::OPEN_ERR_NO_PERM:
								errno = EPERM;  return 0;
							default:
								errno = ENOENT; return 0;
							}
						break;
				case Vfs::Directory_service::OPEN_ERR_NO_PERM: errno = EPERM;  return 0;
				case Vfs::Directory_service::OPEN_ERR_EXISTS:  errno = EEXIST; return 0;
				default:                                       errno = ENOENT; return 0;
				}
		}

		Libc::Plugin_context *context = noux_context(sysio()->open_out.fd);
		Libc::File_descriptor *fd =
		    Libc::file_descriptor_allocator()->alloc(this, context, sysio()->open_out.fd);
		if ((flags & O_TRUNC) && (ftruncate(fd, 0) == -1))
			return 0;
		return fd;
	}


	int Plugin::symlink(const char *oldpath, const char *newpath)
	{
		if (verbose)
			log(__func__, ": ", newpath, " -> ", oldpath);

		if ((Genode::strlen(oldpath) + 1 > sizeof(sysio()->symlink_in.oldpath)) ||
		    (Genode::strlen(newpath) + 1 > sizeof(sysio()->symlink_in.newpath))) {
			log(__func__, ": ENAMETOOLONG");
			errno = ENAMETOOLONG;
			return 0;
		}

		Genode::strncpy(sysio()->symlink_in.oldpath, oldpath, sizeof(sysio()->symlink_in.oldpath));
		Genode::strncpy(sysio()->symlink_in.newpath, newpath, sizeof(sysio()->symlink_in.newpath));
		if (!noux_syscall(Noux::Session::SYSCALL_SYMLINK)) {
			warning("symlink syscall failed for path \"", newpath, "\"");
			typedef Vfs::Directory_service::Symlink_result Result;
			switch (sysio()->error.symlink) {
			case Result::SYMLINK_ERR_NO_ENTRY:      errno = ENOENT;        return -1;
			case Result::SYMLINK_ERR_EXISTS:        errno = EEXIST;        return -1;
			case Result::SYMLINK_ERR_NO_SPACE:      errno = ENOSPC;        return -1;
			case Result::SYMLINK_ERR_NO_PERM:       errno = EPERM;         return -1;
			case Result::SYMLINK_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG;  return -1;
			case Result::SYMLINK_OK: break;
			}
		}

		return 0;
	}


	int Plugin::fstatfs(Libc::File_descriptor *, struct statfs *buf)
	{
		buf->f_flags = MNT_UNION;
		return 0;
	}


	ssize_t Plugin::write(Libc::File_descriptor *fd, const void *buf,
	                      ::size_t count)
	{
		if (!buf) { errno = EFAULT; return -1; }

		/* remember original len for the return value */
		int const orig_count = count;

		char *src = (char *)buf;
		while (count > 0) {

			Genode::size_t curr_count = Genode::min((::size_t)Noux::Sysio::CHUNK_SIZE, count);

			sysio()->write_in.fd = noux_fd(fd->context);
			sysio()->write_in.count = curr_count;
			Genode::memcpy(sysio()->write_in.chunk, src, curr_count);

			if (!noux_syscall(Noux::Session::SYSCALL_WRITE)) {
				switch (sysio()->error.write) {
				case Vfs::File_io_service::WRITE_ERR_AGAIN:       errno = EAGAIN;      break;
				case Vfs::File_io_service::WRITE_ERR_WOULD_BLOCK: errno = EWOULDBLOCK; break;
				case Vfs::File_io_service::WRITE_ERR_INVALID:     errno = EINVAL;      break;
				case Vfs::File_io_service::WRITE_ERR_IO:          errno = EIO;         break;
				case Vfs::File_io_service::WRITE_ERR_INTERRUPT:   errno = EINTR;       break;
				default: 
					if (sysio()->error.general == Vfs::Directory_service::ERR_FD_INVALID)
						errno = EBADF;
					else
						errno = 0;
					break;
				}
			}

			count -= curr_count;
			src   += curr_count;
		}
		return orig_count;
	}


	ssize_t Plugin::read(Libc::File_descriptor *fd, void *buf, ::size_t count)
	{
		if (!buf) { errno = EFAULT; return -1; }

		Genode::size_t sum_read_count = 0;

		while (count > 0) {

			Genode::size_t curr_count =
				Genode::min(count, sizeof(sysio()->read_out.chunk));

			sysio()->read_in.fd    = noux_fd(fd->context);
			sysio()->read_in.count = curr_count;

			if (!noux_syscall(Noux::Session::SYSCALL_READ)) {

				switch (sysio()->error.read) {
				case Vfs::File_io_service::READ_ERR_AGAIN:       errno = EAGAIN;      break;
				case Vfs::File_io_service::READ_ERR_WOULD_BLOCK: errno = EWOULDBLOCK; break;
				case Vfs::File_io_service::READ_ERR_INVALID:     errno = EINVAL;      break;
				case Vfs::File_io_service::READ_ERR_IO:          errno = EIO;         break;
				case Vfs::File_io_service::READ_ERR_INTERRUPT:   errno = EINTR;       break;
				default:
					if (sysio()->error.general == Vfs::Directory_service::ERR_FD_INVALID)
						errno = EBADF;
					else
						errno = 0;
					break;
				}
				return -1;
			}

			Genode::memcpy((char*)buf + sum_read_count,
			               sysio()->read_out.chunk,
			               sysio()->read_out.count);

			sum_read_count += sysio()->read_out.count;

			if (sysio()->read_out.count < curr_count)
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
		if (!noux_syscall(Noux::Session::SYSCALL_CLOSE)) {
			error("close error");
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
		sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_UNDEFINED;

		switch (request) {

		case TIOCGWINSZ:
			sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_TIOCGWINSZ;
			break;

		case TIOCGETA:
			{
				if (verbose)
					log(__func__, ": TIOCGETA - argp=", (void *)argp);
				::termios *termios = (::termios *)argp;

				termios->c_iflag = 0;
				termios->c_oflag = 0;
				termios->c_cflag = 0;
				/*
				 * Set 'ECHO' flag, needed by libreadline. Otherwise, echoing
				 * user input doesn't work in bash.
				 */
				termios->c_lflag = ECHO;
				memset(termios->c_cc, _POSIX_VDISABLE, sizeof(termios->c_cc));
				termios->c_ispeed = 0;
				termios->c_ospeed = 0;

				return 0;
			}

			break;

		case TIOCSETAF:
			{
				sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_TIOCSETAF;

				::termios *termios = (::termios *)argp;

				/**
				 * For now only enabling/disabling of ECHO is supported
				 */
				if (termios->c_lflag & (ECHO | ECHONL)) {
					sysio()->ioctl_in.argp = (Vfs::File_io_service::IOCTL_VAL_ECHO |
					                          Vfs::File_io_service::IOCTL_VAL_ECHONL);
				}
				else {
					sysio()->ioctl_in.argp = Vfs::File_io_service::IOCTL_VAL_NULL;
				}

				break;
			}

		case TIOCSETAW:
			{
				sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_TIOCSETAW;
				sysio()->ioctl_in.argp = argp ? *(int*)argp : 0;

				break;
			}

		case FIONBIO:
			{
				if (verbose)
					log(__func__, ": FIONBIO - *argp=", *argp);

				sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_FIONBIO;
				sysio()->ioctl_in.argp = argp ? *(int*)argp : 0;

				break;
			}

		case DIOCGMEDIASIZE:
			{
				sysio()->ioctl_in.request = Vfs::File_io_service::IOCTL_OP_DIOCGMEDIASIZE;
				sysio()->ioctl_in.argp = 0;

				break;
			}

		default:

			warning("unsupported ioctl (request=", Hex(request), ")");
			break;
		}

		if (sysio()->ioctl_in.request == Vfs::File_io_service::IOCTL_OP_UNDEFINED) {
			errno = ENOTTY;
			return -1;
		}

		/* perform syscall */
		if (!noux_syscall(Noux::Session::SYSCALL_IOCTL)) {
			switch (sysio()->error.ioctl) {
			case Vfs::File_io_service::IOCTL_ERR_INVALID: errno = EINVAL; break;
			case Vfs::File_io_service::IOCTL_ERR_NOTTY:   errno = ENOTTY; break;
			default: errno = 0; break;
			}

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
		case TIOCSETAF:
		case TIOCSETAW:
			return 0;

		case FIONBIO:
			return 0;
		case DIOCGMEDIASIZE:
			{
				int64_t *disk_size = (int64_t*)argp;
				*disk_size = sysio()->ioctl_out.diocgmediasize.size;
				return 0;
			}

		default:
			return -1;
		}
	}


	int Plugin::pipe(Libc::File_descriptor *pipefd[2])
	{
		/* perform syscall */
		if (!noux_syscall(Noux::Session::SYSCALL_PIPE)) {
			error("pipe error");
			/* XXX set errno */
			return -1;
		}

		for (int i = 0; i < 2; i++) {
			Libc::Plugin_context *context = noux_context(sysio()->pipe_out.fd[i]);
			pipefd[i] = Libc::file_descriptor_allocator()->alloc(this, context, sysio()->pipe_out.fd[i]);
		}
		return 0;
	}


	Libc::File_descriptor *Plugin::dup(Libc::File_descriptor* fd)
	{
		sysio()->dup2_in.fd    = noux_fd(fd->context);
		sysio()->dup2_in.to_fd = -1;

		if (!noux_syscall(Noux::Session::SYSCALL_DUP2)) {
			error("dup error");
			/* XXX set errno */
			return 0;
		}

		Libc::Plugin_context *context = noux_context(sysio()->dup2_out.fd);
		return Libc::file_descriptor_allocator()->alloc(this, context,
		                                                sysio()->dup2_out.fd);
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
		if (!noux_syscall(Noux::Session::SYSCALL_DUP2)) {
			error("dup2 error");
			/* XXX set errno */
			return -1;
		}

		return noux_fd(new_fd->context);
	}


	int Plugin::fstat(Libc::File_descriptor *fd, struct stat *buf)
	{
		sysio()->fstat_in.fd = noux_fd(fd->context);
		if (!noux_syscall(Noux::Session::SYSCALL_FSTAT)) {
			error("fstat error");
			/* XXX set errno */
			return -1;
		}

		_sysio_to_stat_struct(sysio(), buf);
		return 0;
	}


	int Plugin::fsync(Libc::File_descriptor *fd)
	{
		if (verbose)
			warning(__func__, ": not implemented");
		return 0;
	}


	int Plugin::ftruncate(Libc::File_descriptor *fd, ::off_t length)
	{
		sysio()->ftruncate_in.fd = noux_fd(fd->context);
		sysio()->ftruncate_in.length = length;
		if (!noux_syscall(Noux::Session::SYSCALL_FTRUNCATE)) {
			switch (sysio()->error.ftruncate) {
			case Vfs::File_io_service::FTRUNCATE_OK: /* never reached */
			case Vfs::File_io_service::FTRUNCATE_ERR_NO_PERM:   errno = EPERM;  break;
			case Vfs::File_io_service::FTRUNCATE_ERR_INTERRUPT: errno = EINTR;  break;
			case Vfs::File_io_service::FTRUNCATE_ERR_NO_SPACE:  errno = ENOSPC; break;
			}
			return -1;
		}

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
				new_fd->path(fd->fd_path);

				/*
				 * Use new allocated number as name of file descriptor
				 * duplicate.
				 */
				if (dup2(fd, new_fd) == -1) {
					error("Plugin::fcntl: dup2 unexpectedly failed");
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
			if (verbose)
				warning("fcntl(F_GETFD) not implemented, returning 0");
			return 0;

		case F_SETFD:
			sysio()->fcntl_in.cmd      = Noux::Sysio::FCNTL_CMD_SET_FD_FLAGS;
			sysio()->fcntl_in.long_arg = arg;
			break;

		case F_GETFL:
			if (verbose)
				log("fcntl: F_GETFL for libc_fd=", fd->libc_fd);
			sysio()->fcntl_in.cmd = Noux::Sysio::FCNTL_CMD_GET_FILE_STATUS_FLAGS;
			break;

		case F_SETFL:
			if (verbose)
				log("fcntl: F_SETFL for libc_fd=", fd->libc_fd);
			sysio()->fcntl_in.cmd      = Noux::Sysio::FCNTL_CMD_SET_FILE_STATUS_FLAGS;
			sysio()->fcntl_in.long_arg = arg;
			break;

		default:
			error("fcntl: unsupported command ", cmd);
			errno = EINVAL;
			return -1;
		};

		/* invoke system call */
		if (!noux_syscall(Noux::Session::SYSCALL_FCNTL)) {
			warning("fcntl failed (libc_fd=", fd->libc_fd, ", cmd=", Hex(cmd), ")");
			switch (sysio()->error.fcntl) {
				case Noux::Sysio::FCNTL_ERR_CMD_INVALID: errno = EINVAL; break;
				default:
					switch (sysio()->error.general) {
					case Vfs::Directory_service::ERR_FD_INVALID: errno = EINVAL; break;
					case Vfs::Directory_service::NUM_GENERAL_ERRORS: break;
					}
			}
			return -1;
		}

		/* read result from sysio */
		return sysio()->fcntl_out.result;
	}


	ssize_t Plugin::getdirentries(Libc::File_descriptor *fd, char *buf,
	                              ::size_t nbytes, ::off_t *basep)
	{
		if (nbytes < sizeof(struct dirent)) {
			error("buf too small");
			return -1;
		}

		sysio()->dirent_in.fd = noux_fd(fd->context);

		struct dirent *dirent = (struct dirent *)buf;
		Genode::memset(dirent, 0, sizeof(struct dirent));

		if (!noux_syscall(Noux::Session::SYSCALL_DIRENT)) {
			switch (sysio()->error.general) {

			case Vfs::Directory_service::ERR_FD_INVALID:
				errno = EBADF;
				error("dirent: ERR_FD_INVALID");
				return -1;

			case Vfs::Directory_service::NUM_GENERAL_ERRORS: return -1;
			}
		}

		switch (sysio()->dirent_out.entry.type) {
		case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY: dirent->d_type = DT_DIR;  break;
		case Vfs::Directory_service::DIRENT_TYPE_FILE:      dirent->d_type = DT_REG;  break;
		case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:   dirent->d_type = DT_LNK;  break;
		case Vfs::Directory_service::DIRENT_TYPE_FIFO:      dirent->d_type = DT_FIFO; break;
		case Vfs::Directory_service::DIRENT_TYPE_CHARDEV:   dirent->d_type = DT_CHR; break;
		case Vfs::Directory_service::DIRENT_TYPE_BLOCKDEV:  dirent->d_type = DT_BLK; break;
		case Vfs::Directory_service::DIRENT_TYPE_END:       return 0;
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

		if (!noux_syscall(Noux::Session::SYSCALL_LSEEK)) {
			switch (sysio()->error.general) {

			case Vfs::Directory_service::ERR_FD_INVALID:
				errno = EBADF;
				error("lseek: ERR_FD_INVALID");
				return -1;

			case Vfs::Directory_service::NUM_GENERAL_ERRORS: return -1;
			}
		}

		return sysio()->lseek_out.offset;
	}


	int Plugin::unlink(char const *path)
	{
		Genode::strncpy(sysio()->unlink_in.path, path, sizeof(sysio()->unlink_in.path));

		if (!noux_syscall(Noux::Session::SYSCALL_UNLINK)) {
			warning("unlink syscall failed for path \"", path, "\"");
			typedef Vfs::Directory_service::Unlink_result Result;
			switch (sysio()->error.unlink) {
			case Result::UNLINK_ERR_NO_ENTRY:  errno = ENOENT;    break;
			case Result::UNLINK_ERR_NOT_EMPTY: errno = ENOTEMPTY; break;
			case Result::UNLINK_ERR_NO_PERM:   errno = EPERM;     break;
			case Result::UNLINK_OK: break; /* only here to complete the enumeration */
			}
			return -1;
		}

		return 0;
	}


	int Plugin::rmdir(char const *path)
	{
		return Plugin::unlink(path);
	}


	ssize_t Plugin::readlink(const char *path, char *buf, ::size_t bufsiz)
	{
		if (verbose)
			log(__func__, ": path=", path, ", bufsiz=", bufsiz);

		Genode::strncpy(sysio()->readlink_in.path, path, sizeof(sysio()->readlink_in.path));
		sysio()->readlink_in.bufsiz = bufsiz;

		if (!noux_syscall(Noux::Session::SYSCALL_READLINK)) {
			warning("readlink syscall failed for path \"", path, "\"");
			typedef Vfs::Directory_service::Readlink_result Result;
			switch (sysio()->error.readlink) {
			case Result::READLINK_ERR_NO_ENTRY: errno = ENOENT; return -1;
			case Result::READLINK_ERR_NO_PERM:  errno = EPERM;  return -1;
			case Result::READLINK_OK: break;
			}
		}

		ssize_t size = Genode::min((size_t)sysio()->readlink_out.count, bufsiz);

		Genode::memcpy(buf, sysio()->readlink_out.chunk, size);

		if (verbose)
			log(__func__, ": result=", Genode::Cstring(buf));

		return size;
	}


	int Plugin::rename(char const *from_path, char const *to_path)
	{
		Genode::strncpy(sysio()->rename_in.from_path, from_path, sizeof(sysio()->rename_in.from_path));
		Genode::strncpy(sysio()->rename_in.to_path,   to_path,   sizeof(sysio()->rename_in.to_path));

		if (!noux_syscall(Noux::Session::SYSCALL_RENAME)) {
			warning("rename syscall failed for \"", from_path, "\" -> \"", to_path, "\"");
			switch (sysio()->error.rename) {
			case Vfs::Directory_service::RENAME_ERR_NO_ENTRY: errno = ENOENT; break;
			case Vfs::Directory_service::RENAME_ERR_CROSS_FS: errno = EXDEV;  break;
			case Vfs::Directory_service::RENAME_ERR_NO_PERM:  errno = EPERM;  break;
			default:                                          errno = EPERM;  break;
			}
			return -1;
		}

		return 0;
	}


	int Plugin::mkdir(const char *path, mode_t mode)
	{
		Genode::strncpy(sysio()->mkdir_in.path, path, sizeof(sysio()->mkdir_in.path));

		if (!noux_syscall(Noux::Session::SYSCALL_MKDIR)) {
			warning("mkdir syscall failed for \"", path, "\" mode=", Hex(mode));
			switch (sysio()->error.mkdir) {
			case Vfs::Directory_service::MKDIR_ERR_EXISTS:        errno = EEXIST;       break;
			case Vfs::Directory_service::MKDIR_ERR_NO_ENTRY:      errno = ENOENT;       break;
			case Vfs::Directory_service::MKDIR_ERR_NO_SPACE:      errno = ENOSPC;       break;
			case Vfs::Directory_service::MKDIR_ERR_NAME_TOO_LONG: errno = ENAMETOOLONG; break;
			case Vfs::Directory_service::MKDIR_ERR_NO_PERM:       errno = EPERM;        break;
			default:                                              errno = EPERM;        break;
			}
			return -1;
		}
		return 0;
	}


	void *Plugin::mmap(void *addr_in, ::size_t length, int prot, int flags,
	                   Libc::File_descriptor *fd, ::off_t offset)
	{
		if (prot != PROT_READ) {
			error("mmap for prot=", Hex(prot), " not supported");
			errno = EACCES;
			return (void *)-1;
		}

		if (addr_in != 0) {
			error("mmap for predefined address not supported");
			errno = EINVAL;
			return (void *)-1;
		}

		void *addr = Libc::mem_alloc()->alloc(length, PAGE_SHIFT);
		if (addr == (void *)-1) {
			errno = ENOMEM;
			return (void *)-1;
		}

		if (::pread(fd->libc_fd, addr, length, offset) < 0) {
			error("mmap could not obtain file content");
			::munmap(addr, length);
			errno = EACCES;
			return (void *)-1;
		}

		return addr;
	}


	int Plugin::munmap(void *addr, ::size_t)
	{
		Libc::mem_alloc()->free(addr);
		return 0;
	}


	Libc::File_descriptor *Plugin::socket(int domain, int type, int protocol)
	{
		sysio()->socket_in.domain = domain;
		sysio()->socket_in.type = type;
		sysio()->socket_in.protocol = protocol;

		if (!noux_syscall(Noux::Session::SYSCALL_SOCKET))
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

		if (!noux_syscall(Noux::Session::SYSCALL_GETSOCKOPT))
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

		if (!noux_syscall(Noux::Session::SYSCALL_SETSOCKOPT)) {
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

		if (!noux_syscall(Noux::Session::SYSCALL_ACCEPT)) {
			switch (sysio()->error.accept) {
			case Noux::Sysio::ACCEPT_ERR_AGAIN:         errno = EAGAIN;      break;
			case Noux::Sysio::ACCEPT_ERR_NO_MEMORY:     errno = ENOMEM;      break;
			case Noux::Sysio::ACCEPT_ERR_INVALID:       errno = EINVAL;      break;
			case Noux::Sysio::ACCEPT_ERR_NOT_SUPPORTED: errno = EOPNOTSUPP;  break;
			case Noux::Sysio::ACCEPT_ERR_WOULD_BLOCK:   errno = EWOULDBLOCK; break;
			default:                                    errno = 0;           break;
			}

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

		if (!noux_syscall(Noux::Session::SYSCALL_BIND)) {
			switch (sysio()->error.bind) {
			case Noux::Sysio::BIND_ERR_ACCESS:      errno = EACCES;     break;
			case Noux::Sysio::BIND_ERR_ADDR_IN_USE: errno = EADDRINUSE; break;
			case Noux::Sysio::BIND_ERR_INVALID:     errno = EINVAL;     break;
			case Noux::Sysio::BIND_ERR_NO_MEMORY:   errno = ENOMEM;     break;
			default:                                errno = 0;          break;
			}
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

		if (!noux_syscall(Noux::Session::SYSCALL_CONNECT)) {
			switch (sysio()->error.connect) {
			case Noux::Sysio::CONNECT_ERR_AGAIN:        errno = EAGAIN;       break;
			case Noux::Sysio::CONNECT_ERR_ALREADY:      errno = EALREADY;     break;
			case Noux::Sysio::CONNECT_ERR_ADDR_IN_USE:  errno = EADDRINUSE;   break;
			case Noux::Sysio::CONNECT_ERR_IN_PROGRESS:  errno = EINPROGRESS;  break;
			case Noux::Sysio::CONNECT_ERR_IS_CONNECTED: errno = EISCONN;      break;
			case Noux::Sysio::CONNECT_ERR_RESET:        errno = ECONNRESET;   break;
			case Noux::Sysio::CONNECT_ERR_ABORTED:      errno = ECONNABORTED; break;
			case Noux::Sysio::CONNECT_ERR_NO_ROUTE:     errno = EHOSTUNREACH; break;
			default:                                    errno = 0;            break;
			}
			return -1;
		}

		return 0;
	}


	int Plugin::getpeername(Libc::File_descriptor *fd, struct sockaddr *addr,
			socklen_t *addrlen)
	{
		sysio()->getpeername_in.fd = noux_fd(fd->context);
		sysio()->getpeername_in.addrlen = *addrlen;

		if (!noux_syscall(Noux::Session::SYSCALL_GETPEERNAME)) {
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

		if (!noux_syscall(Noux::Session::SYSCALL_LISTEN)) {
			switch (sysio()->error.listen) {
			case Noux::Sysio::LISTEN_ERR_ADDR_IN_USE:   errno = EADDRINUSE; break;
			case Noux::Sysio::LISTEN_ERR_NOT_SUPPORTED: errno = EOPNOTSUPP; break;
			default:                                    errno = 0;          break;
			}
			return -1;
		}

		return 0;
	}


	ssize_t Plugin::recv(Libc::File_descriptor *fd, void *buf, ::size_t len, int flags)
	{
		if (!buf) { errno = EFAULT; return -1; }

		Genode::size_t sum_recv_count = 0;

		while (len > 0) {
			Genode::size_t curr_len =
				Genode::min(len, sizeof(sysio()->recv_in.buf));

			sysio()->recv_in.fd = noux_fd(fd->context);
			sysio()->recv_in.len = curr_len;

			if (!noux_syscall(Noux::Session::SYSCALL_RECV)) {
				switch (sysio()->error.recv) {
				case Noux::Sysio::RECV_ERR_AGAIN:         errno = EAGAIN;      break;
				case Noux::Sysio::RECV_ERR_WOULD_BLOCK:   errno = EWOULDBLOCK; break;
				case Noux::Sysio::RECV_ERR_INVALID:       errno = EINVAL;      break;
				case Noux::Sysio::RECV_ERR_NOT_CONNECTED: errno = ENOTCONN;    break;
				default:                                  errno = 0;           break;
				}
				return -1;
			}

			Genode::memcpy((char *)buf + sum_recv_count,
			               sysio()->recv_in.buf, sysio()->recv_out.len);

			sum_recv_count += sysio()->recv_out.len;

			if (sysio()->recv_out.len < curr_len)
				break;

			if (sysio()->recv_out.len <= len)
				len -= sysio()->recv_out.len;
			else
				break;
		}

		return sum_recv_count;
	}


	ssize_t Plugin::recvfrom(Libc::File_descriptor *fd, void *buf, ::size_t len, int flags,
	                         struct sockaddr *src_addr, socklen_t *addrlen)
	{
		if (!buf) { errno = EFAULT; return -1; }

		Genode::size_t sum_recvfrom_count = 0;

		while (len > 0) {
			Genode::size_t curr_len = Genode::min(len, sizeof(sysio()->recvfrom_in.buf));

			sysio()->recv_in.fd = noux_fd(fd->context);
			sysio()->recv_in.len = curr_len;

			if (addrlen == NULL)
				sysio()->recvfrom_in.addrlen = 0;
			else
				sysio()->recvfrom_in.addrlen = *addrlen;

			if (!noux_syscall(Noux::Session::SYSCALL_RECVFROM)) {
				switch (sysio()->error.recv) {
				case Noux::Sysio::RECV_ERR_AGAIN:         errno = EAGAIN;      break;
				case Noux::Sysio::RECV_ERR_WOULD_BLOCK:   errno = EWOULDBLOCK; break;
				case Noux::Sysio::RECV_ERR_INVALID:       errno = EINVAL;      break;
				case Noux::Sysio::RECV_ERR_NOT_CONNECTED: errno = ENOTCONN;    break;
				default:                                  errno = 0;           break;
				}
				return -1;
			}

			if (src_addr != NULL && addrlen != NULL)
				Genode::memcpy(src_addr, &sysio()->recvfrom_in.src_addr,
					       sysio()->recvfrom_in.addrlen);


			Genode::memcpy((char *)buf + sum_recvfrom_count,
			               sysio()->recvfrom_in.buf, sysio()->recvfrom_out.len);

			sum_recvfrom_count += sysio()->recvfrom_out.len;

			if (sysio()->recvfrom_out.len < curr_len)
				break;

			if (sysio()->recvfrom_out.len <= len)
				len -= sysio()->recvfrom_out.len;
			else
				break;
		}

		return sum_recvfrom_count;
	}


	ssize_t Plugin::send(Libc::File_descriptor *fd, const void *buf, ::size_t len, int flags)
	{
		if (!buf) { errno = EFAULT; return -1; }

		/* remember original len for the return value */
		int const orig_count = len;
		char *src = (char *)buf;

		while (len > 0) {

			Genode::size_t curr_len = Genode::min(sizeof (sysio()->send_in.buf), len);

			sysio()->send_in.fd = noux_fd(fd->context);
			sysio()->send_in.len = curr_len;
			Genode::memcpy(sysio()->send_in.buf, src, curr_len);

			if (!noux_syscall(Noux::Session::SYSCALL_SEND)) {
				error("write error ", (int)sysio()->error.general);
				switch (sysio()->error.send) {
				case Noux::Sysio::SEND_ERR_AGAIN:            errno = EAGAIN;      break;
				case Noux::Sysio::SEND_ERR_WOULD_BLOCK:      errno = EWOULDBLOCK; break;
				case Noux::Sysio::SEND_ERR_CONNECTION_RESET: errno = ECONNRESET;  break;
				case Noux::Sysio::SEND_ERR_INVALID:          errno = EINVAL;      break;
				case Noux::Sysio::SEND_ERR_IS_CONNECTED:     errno = EISCONN;     break;
				case Noux::Sysio::SEND_ERR_NO_MEMORY:        errno = ENOMEM;      break;
				default:                                     errno = 0;           break;
				}
				/* return foo */
			}

			len -= curr_len;
			src += curr_len;
		}
		return orig_count;
	}


	ssize_t Plugin::sendto(Libc::File_descriptor *fd, const void *buf, size_t len, int flags,
			const struct sockaddr *dest_addr, socklen_t addrlen)
	{
		if (!buf) { errno = EFAULT; return -1; }

		int const orig_count = len;

		if (addrlen > sizeof (sysio()->sendto_in.dest_addr)) {
			errno = 0; /* XXX */
			return -1;
		}

		/* wipe-out sendto buffer */
		Genode::memset(sysio()->sendto_in.buf, 0, sizeof (sysio()->sendto_in.buf));

		char *src = (char *)buf;
		while (len > 0) {
			size_t curr_len = Genode::min(sizeof (sysio()->sendto_in.buf), len);

			sysio()->sendto_in.fd = noux_fd(fd->context);
			sysio()->sendto_in.len = curr_len;
			Genode::memcpy(sysio()->sendto_in.buf, src, curr_len);

			if (addrlen == 0) {
				sysio()->sendto_in.addrlen = 0;
				Genode::memset(&sysio()->sendto_in.dest_addr, 0, sizeof (struct sockaddr));
			}
			else {
				sysio()->sendto_in.addrlen = addrlen;
				Genode::memcpy(&sysio()->sendto_in.dest_addr, dest_addr, addrlen);
			}

			if (!noux_syscall(Noux::Session::SYSCALL_SENDTO)) {
				switch (sysio()->error.send) {
				case Noux::Sysio::SEND_ERR_AGAIN:            errno = EAGAIN;      break;
				case Noux::Sysio::SEND_ERR_WOULD_BLOCK:      errno = EWOULDBLOCK; break;
				case Noux::Sysio::SEND_ERR_CONNECTION_RESET: errno = ECONNRESET;  break;
				case Noux::Sysio::SEND_ERR_INVALID:          errno = EINVAL;      break;
				case Noux::Sysio::SEND_ERR_IS_CONNECTED:     errno = EISCONN;     break;
				case Noux::Sysio::SEND_ERR_NO_MEMORY:        errno = ENOMEM;      break;
				default:                                     errno = 0;           break;
				}
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

		if (!noux_syscall(Noux::Session::SYSCALL_SHUTDOWN)) {
			switch (sysio()->error.shutdown) {
			case Noux::Sysio::SHUTDOWN_ERR_NOT_CONNECTED: errno = ENOTCONN; break;
			default:                                      errno = 0;        break;
			}
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
	sigemptyset(&signal_mask);

	/* copy command-line arguments from 'args' ROM dataspace */
	enum { MAX_ARGS = 256, ARG_BUF_SIZE = 4096UL };
	static char *argv[MAX_ARGS];
	static char  arg_buf[ARG_BUF_SIZE];
	{
		Genode::Attached_rom_dataspace ds("args");
		Genode::memcpy(arg_buf, ds.local_addr<char>(),
		               Genode::min((size_t)ARG_BUF_SIZE, ds.size()));
	}

	int argc = 0;
	for (unsigned i = 0; arg_buf[i] && (i < ARG_BUF_SIZE - 2); ) {

		if (i >= ARG_BUF_SIZE - 2) {
			warning("command-line argument buffer exceeded");
			break;
		}

		if (argc >= MAX_ARGS - 1) {
			warning("number of command-line arguments exceeded");
			break;
		}

		argv[argc] = &arg_buf[i];
		i += Genode::strlen(&arg_buf[i]) + 1; /* skip null-termination */
		argc++;
	}

	/* register command-line arguments at Genode's startup code */
	genode_argv = argv;
	genode_argc = argc;

	/*
	 * Make environment variables from 'env' ROM dataspace available to libc's
	 * 'environ'.
	 */
	enum { ENV_MAX_ENTRIES = 128, ENV_BUF_SIZE = 8*1024 };
	static char *env_array[ENV_MAX_ENTRIES];
	static char  env_buf[ENV_BUF_SIZE];
	{
		Genode::Attached_rom_dataspace ds("env");
		Genode::memcpy(env_buf, ds.local_addr<char>(),
		               Genode::min((size_t)ENV_BUF_SIZE, ds.size()));
	}
	char *env_string = &env_buf[0];

	unsigned num_entries = 0;  /* index within 'env_array' */

	static Libc::Absolute_path noux_cwd("/");

	while (*env_string && (num_entries < ENV_MAX_ENTRIES - 1)) {
		if ((strlen(env_string) >= strlen("NOUX_CWD=")) &&
		    (strncmp(env_string, "NOUX_CWD=", strlen("NOUX_CWD=")) == 0)) {
			noux_cwd.import(&env_string[strlen("NOUX_CWD=")]);
		} else {
			env_array[num_entries++] = env_string;
		}
		env_string += (strlen(env_string) + 1);
	}
	env_array[num_entries] = 0;

	/* register list of environment variables at libc 'environ' pointer */
	environ = env_array;

	/* define env pointer to be passed to main function (in '_main.cc') */
	genode_envp = environ;

	/* initialize noux libc plugin */
	static Plugin noux_plugin;

	chdir(noux_cwd.base());

	/*
	 * Enhance main-thread stack
	 *
	 * This is done because we ran into a stack overflow while compiling
	 * Genodes core/main.cc with GCC in Noux.
	 */
	enum { STACK_SIZE = 32UL * 1024 * sizeof(Genode::addr_t) };
	Genode::Thread::myself()->stack_size(STACK_SIZE);
}


