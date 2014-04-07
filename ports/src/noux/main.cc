/*
 * \brief  Unix emulation environment for Genode
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cap_session/connection.h>
#include <os/config.h>
#include <os/alarm.h>
#include <timer_session/connection.h>

/* Noux includes */
#include <child.h>
#include <child_env.h>
#include <noux_session/sysio.h>
#include <vfs_io_channel.h>
#include <terminal_io_channel.h>
#include <dummy_input_io_channel.h>
#include <pipe_io_channel.h>
#include <dir_file_system.h>
#include <user_info.h>
#include <io_receptor_registry.h>
#include <destruct_queue.h>
#include <kill_broadcaster.h>
#include <file_system_registry.h>

/* supported file systems */
#include <tar_file_system.h>
#include <fs_file_system.h>
#include <terminal_file_system.h>
#include <null_file_system.h>
#include <zero_file_system.h>
#include <stdio_file_system.h>
#include <random_file_system.h>
#include <block_file_system.h>


static const bool verbose_quota  = false;
static bool trace_syscalls = false;
static bool verbose = false;

namespace Noux {

	static Noux::Child *init_child;

	bool is_init_process(Child *child) { return child == init_child; }
	void init_process_exited() { init_child = 0; }

};

extern void init_network();

/**
 * Timeout thread for SYSCALL_SELECT
 */

namespace Noux {
	using namespace Genode;

	class Timeout_scheduler : Thread<4096>, public Alarm_scheduler
	{
		private:
			Timer::Connection _timer;
			Alarm::Time       _curr_time;

			enum { TIMER_GRANULARITY_MSEC = 10 };

			void entry()
			{
				for (;;) {
					_timer.msleep(TIMER_GRANULARITY_MSEC);
					Alarm_scheduler::handle(_curr_time);
					_curr_time += TIMER_GRANULARITY_MSEC;
				}
			}

		public:
			Timeout_scheduler(unsigned long curr_time)
			: Thread("timeout_sched"), _curr_time(curr_time) { start(); }

			Alarm::Time curr_time() const { return _curr_time; }
	};

	struct Timeout_state
	{
		bool timed_out;

		Timeout_state() : timed_out(false)  { }
	};

	class Timeout_alarm : public Alarm
	{
		private:
			Timeout_state     *_state;
			Lock              *_blocker;
			Timeout_scheduler *_scheduler;

		public:
			Timeout_alarm(Timeout_state *st, Lock *blocker, Timeout_scheduler *scheduler, Time timeout)
				:
					_state(st),
					_blocker(blocker),
					_scheduler(scheduler)
		{
			_scheduler->schedule_absolute(this, _scheduler->curr_time() + timeout);
			_state->timed_out = false;
		}

			void discard() { _scheduler->discard(this); }

		protected:
			bool on_alarm()
			{
				_state->timed_out = true;
				_blocker->unlock();

				return false;
			}
	};

	/**
	 * This function is used to generate inode values from the given
	 * path using the FNV-1a algorithm.
	 */
	inline uint32_t hash_path(const char *path, size_t len)
	{
		const unsigned char * p = reinterpret_cast<const unsigned char*>(path);
		uint32_t hash = 2166136261U;

		for (size_t i = 0; i < len; i++) {
			hash ^= p[i];
			hash *= 16777619;
		}

		return hash;
	}
};


/*****************************
 ** Noux syscall dispatcher **
 *****************************/

bool Noux::Child::syscall(Noux::Session::Syscall sc)
{
	if (trace_syscalls)
		Genode::printf("PID %d -> SYSCALL %s\n",
		               pid(), Noux::Session::syscall_name(sc));

	bool result = false;

	try {
		switch (sc) {

		case SYSCALL_WRITE:
			{
				size_t const count_in = _sysio->write_in.count;

				for (size_t count = 0; count != count_in; ) {

					Shared_pointer<Io_channel> io = _lookup_channel(_sysio->write_in.fd);

					if (!io->is_nonblocking())
						_block_for_io_channel(io, false, true, false);

					if (io->check_unblock(false, true, false)) {
						/*
						 * 'io->write' is expected to update
						 * '_sysio->write_out.count' and 'count'
						 */
						result = io->write(_sysio, count);
						if (result == false)
							break;
					} else {
						if (result == false) {
							/* nothing was written yet */
							_sysio->error.write = Sysio::WRITE_ERR_INTERRUPT;
						}
						break;
					}
				}

				break;
			}

		case SYSCALL_READ:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio->read_in.fd);

				if (!io->is_nonblocking())
					_block_for_io_channel(io, true, false, false);

				if (io->check_unblock(true, false, false))
					result = io->read(_sysio);
				else
					_sysio->error.read = Sysio::READ_ERR_INTERRUPT;

				break;
			}

		case SYSCALL_FTRUNCATE:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio->ftruncate_in.fd);

				_block_for_io_channel(io, false, true, false);

				if (io->check_unblock(false, true, false))
					result = io->ftruncate(_sysio);
				else
					_sysio->error.ftruncate = Sysio::FTRUNCATE_ERR_INTERRUPT;

				break;
			}

		case SYSCALL_STAT:
		case SYSCALL_LSTAT: /* XXX implement difference between 'lstat' and 'stat' */
			{
				/**
				 * We calculate the inode by hashing the path because there is
				 * no inode registry in noux.
				 */
				size_t   path_len  = strlen(_sysio->stat_in.path);
				uint32_t path_hash = hash_path(_sysio->stat_in.path, path_len);

				result = root_dir()->stat(_sysio, _sysio->stat_in.path);

				/**
				 * Instead of using the uid/gid given by the actual file system
				 * we use the ones specificed in the config.
				 */
				if (result) {
					_sysio->stat_out.st.uid = user_info()->uid;
					_sysio->stat_out.st.gid = user_info()->gid;

					_sysio->stat_out.st.inode = path_hash;
				}

				break;
			}

		case SYSCALL_FSTAT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio->fstat_in.fd);

				result = io->fstat(_sysio);

				if (result) {
					Sysio::Path path;

					/**
					 * Only actual fd's are valid fstat targets.
					 */
					if (io->path(path, sizeof (path))) {
						size_t path_len    = strlen(path);
						uint32_t path_hash = hash_path(path, path_len);

						_sysio->stat_out.st.inode = path_hash;
					}
				}

				break;
			}

		case SYSCALL_FCNTL:

			if (_sysio->fcntl_in.cmd == Sysio::FCNTL_CMD_SET_FD_FLAGS) {

				/* we assume that there is only the close-on-execve flag */
				_lookup_channel(_sysio->fcntl_in.fd)->close_on_execve =
					!!_sysio->fcntl_in.long_arg;
				result = true;
				break;
			}

			result = _lookup_channel(_sysio->fcntl_in.fd)->fcntl(_sysio);
			break;

		case SYSCALL_OPEN:
			{
				Vfs_handle *vfs_handle = root_dir()->open(_sysio, _sysio->open_in.path);
				if (!vfs_handle)
					break;

				char const *leaf_path = root_dir()->leaf_path(_sysio->open_in.path);

				/*
				 * File descriptors of opened directories are handled by
				 * '_root_dir'. In this case, we use the absolute path as leaf
				 * path because path operations always refer to the global
				 * root.
				 */
				if (vfs_handle->ds() == root_dir())
					leaf_path = _sysio->open_in.path;

				Shared_pointer<Io_channel>
					channel(new Vfs_io_channel(_sysio->open_in.path,
					                           leaf_path, root_dir(),
					                           vfs_handle, *_sig_rec),
					        Genode::env()->heap());

				_sysio->open_out.fd = add_io_channel(channel);
				result = true;
				break;
			}

		case SYSCALL_CLOSE:
			{
				remove_io_channel(_sysio->close_in.fd);
				result = true;
				break;
			}

		case SYSCALL_IOCTL:

			result = _lookup_channel(_sysio->ioctl_in.fd)->ioctl(_sysio);
			break;

		case SYSCALL_LSEEK:

			result = _lookup_channel(_sysio->lseek_in.fd)->lseek(_sysio);
			break;

		case SYSCALL_DIRENT:

			result = _lookup_channel(_sysio->dirent_in.fd)->dirent(_sysio);
			break;

		case SYSCALL_EXECVE:
			{
				/*
				 * We have to check the dataspace twice because the binary
				 * could be a script that uses an interpreter which maybe
				 * does not exist.
				 */
				Dataspace_capability binary_ds =
					root_dir()->dataspace(_sysio->execve_in.filename);

				if (!binary_ds.valid()) {
					_sysio->error.execve = Sysio::EXECVE_NONEXISTENT;
					break;
				}

				Child_env<sizeof(_sysio->execve_in.args)>
					child_env(_sysio->execve_in.filename, binary_ds,
					          _sysio->execve_in.args, _sysio->execve_in.env);

				root_dir()->release(_sysio->execve_in.filename, binary_ds);

				binary_ds = root_dir()->dataspace(child_env.binary_name());

				if (!binary_ds.valid()) {
					_sysio->error.execve = Sysio::EXECVE_NONEXISTENT;
					break;
				}

				root_dir()->release(child_env.binary_name(), binary_ds);

				try {
					_parent_execve.execve_child(*this,
					                            child_env.binary_name(),
					                            child_env.args(),
					                            child_env.env(),
					                            verbose);

					/*
					 * 'return' instead of 'break' to skip possible signal delivery,
					 * which might cause the old child process to exit itself
					 */
					return true;
				}
				catch (Child::Binary_does_not_exist) {
					_sysio->error.execve = Sysio::EXECVE_NONEXISTENT; }

				break;
			}

		case SYSCALL_SELECT:
			{
				Sysio::Select_fds &in_fds = _sysio->select_in.fds;
				size_t in_fds_total = in_fds.total_fds();

				int _rd_array[in_fds_total];
				int _wr_array[in_fds_total];

				long timeout_sec     = _sysio->select_in.timeout.sec;
				long timeout_usec    = _sysio->select_in.timeout.usec;
				bool timeout_reached = false;

				/* reset the blocker lock to the 'locked' state */
				_blocker.unlock();
				_blocker.lock();

				/*
				 * Register ourself at all watched I/O channels
				 *
				 * We instantiate as many notifiers as we have file
				 * descriptors to observe. Each notifier is associated
				 * with the child's blocking semaphore. When any of the
				 * notifiers get woken up, the semaphore gets unblocked.
				 *
				 * XXX However, the blocker may get unblocked for other
				 *     conditions such as the destruction of the child.
				 *     ...to be done.
				 */

				Wake_up_notifier notifiers[in_fds_total];

				for (Genode::size_t i = 0; i < in_fds_total; i++) {
					int fd = in_fds.array[i];
					if (!fd_in_use(fd)) continue;

					Shared_pointer<Io_channel> io = io_channel_by_fd(fd);
					notifiers[i].lock = &_blocker;

					io->register_wake_up_notifier(&notifiers[i]);
				}

				/**
				 * Register ourself at the Io_receptor_registry
				 *
				 * Each entry in the registry will be unblocked if an external
				 * event has happend, e.g. network I/O.
				 */

				Io_receptor receptor(&_blocker);
				io_receptor_registry()->register_receptor(&receptor);


				/*
				 * Block for one action of the watched file descriptors
				 */
				for (;;) {

					/*
					 * Check I/O channels of specified file descriptors for
					 * unblock condition. Return if one I/O channel satisfies
					 * the condition.
					 */
					size_t unblock_rd = 0;
					size_t unblock_wr = 0;
					size_t unblock_ex = 0;
					
					/* process read fds */
					for (Genode::size_t i = 0; i < in_fds_total; i++) {

						int fd = in_fds.array[i];
						if (!fd_in_use(fd)) continue;

						Shared_pointer<Io_channel> io = io_channel_by_fd(fd);

						if (in_fds.watch_for_rd(i))
							if (io->check_unblock(true, false, false)) {
								_rd_array[unblock_rd++] = fd;
							}
						if (in_fds.watch_for_wr(i))
							if (io->check_unblock(false, true, false)) {
								_wr_array[unblock_wr++] = fd;
							}
						if (in_fds.watch_for_ex(i))
							if (io->check_unblock(false, false, true)) {
								unblock_ex++;
							}
					}

					if (unblock_rd || unblock_wr || unblock_ex) {
						/**
						 * Merge the fd arrays in one output array
						 */
						for (size_t i = 0; i < unblock_rd; i++) {
							_sysio->select_out.fds.array[i] = _rd_array[i];
						}
						_sysio->select_out.fds.num_rd = unblock_rd;

						/* XXX could use a pointer to select_out.fds.array instead */
						for (size_t j = unblock_rd, i = 0; i < unblock_wr; i++, j++) {
							_sysio->select_out.fds.array[j] = _wr_array[i];
						}
						_sysio->select_out.fds.num_wr = unblock_wr;

						/* exception fds are currently not considered */
						_sysio->select_out.fds.num_ex = unblock_ex;

						result = true;
						break;
					}

					/*
					 * Return if timeout is zero or timeout exceeded
					 */

					if (_sysio->select_in.timeout.zero() || timeout_reached) {
						/*
						if (timeout_reached) PINF("timeout_reached");
						else                 PINF("timeout.zero()");
						*/
						_sysio->select_out.fds.num_rd = 0;
						_sysio->select_out.fds.num_wr = 0;
						_sysio->select_out.fds.num_ex = 0;

						result = true;
						break;
					}

					/*
					 * Return if signals are pending
					 */

					if (!_pending_signals.empty()) {
						_sysio->error.select = Sysio::SELECT_ERR_INTERRUPT;
						break;
					}

					/*
					 * Block at barrier except when reaching the timeout
					 */

					if (!_sysio->select_in.timeout.infinite()) {
						unsigned long to_msec = (timeout_sec * 1000) + (timeout_usec / 1000);
						Timeout_state ts;
						Timeout_alarm ta(&ts, &_blocker, Noux::timeout_scheduler(), to_msec);

						/* block until timeout is reached or we were unblocked */
						_blocker.lock();

						if (ts.timed_out) {
							timeout_reached = 1;
						}
						else {
							/*
							 * We woke up before reaching the timeout,
							 * so we discard the alarm
							 */
							ta.discard();
						}
					}
					else {
						/* let's block infinitely */
						_blocker.lock();
					}

				}

				/*
				 * Unregister barrier at watched I/O channels
				 */
				for (Genode::size_t i = 0; i < in_fds_total; i++) {
					int fd = in_fds.array[i];
					if (!fd_in_use(fd)) continue;

					Shared_pointer<Io_channel> io = io_channel_by_fd(fd);
					io->unregister_wake_up_notifier(&notifiers[i]);
				}

				/*
				 * Unregister receptor
				 */
				io_receptor_registry()->unregister_receptor(&receptor);

				break;
			}

		case SYSCALL_FORK:
			{
				Genode::addr_t ip              = _sysio->fork_in.ip;
				Genode::addr_t sp              = _sysio->fork_in.sp;
				Genode::addr_t parent_cap_addr = _sysio->fork_in.parent_cap_addr;

				int const new_pid = pid_allocator()->alloc();

				/*
				 * XXX To ease debugging, it would be useful to generate a
				 *     unique name that includes the PID instead of just
				 *     reusing the name of the parent.
				 */
				Child *child = new Child(_child_policy.name(),
				                         this,
				                         _kill_broadcaster,
				                         *this,
				                         new_pid,
				                         _sig_rec,
				                         root_dir(),
				                         _args,
				                         _env.env(),
				                         _cap_session,
				                         _parent_services,
				                         _resources.ep,
				                         true,
				                         env()->heap(),
				                         _destruct_queue,
				                         verbose);

				Family_member::insert(child);

				_assign_io_channels_to(child);

				/* copy our address space into the new child */
				_resources.rm.replay(child->ram(), child->rm(),
				                     child->ds_registry(), _resources.ep);

				/* start executing the main thread of the new process */
				child->start_forked_main_thread(ip, sp, parent_cap_addr);

				/* activate child entrypoint, thereby starting the new process */
				child->start();

				_sysio->fork_out.pid = new_pid;

				result = true;
				break;
			}

		case SYSCALL_GETPID:
			{
				_sysio->getpid_out.pid = pid();
				return true;
			}

		case SYSCALL_WAIT4:
			{
				Family_member *exited = _sysio->wait4_in.nohang ? poll4() : wait4();

				if (exited) {
					_sysio->wait4_out.pid    = exited->pid();
					_sysio->wait4_out.status = exited->exit_status();
					Family_member::remove(exited);

					if (verbose)
						PINF("submit exit signal for PID %d", exited->pid());
					static_cast<Child *>(exited)->submit_exit_signal();

				} else {
					if (_sysio->wait4_in.nohang) {
						_sysio->wait4_out.pid    = 0;
						_sysio->wait4_out.status = 0;
					} else {
						_sysio->error.wait4 = Sysio::WAIT4_ERR_INTERRUPT;
						break;
					}
				}
				result = true;
				break;
			}

		case SYSCALL_PIPE:
			{
				Shared_pointer<Pipe> pipe(new Pipe, Genode::env()->heap());

				Shared_pointer<Io_channel> pipe_sink(new Pipe_sink_io_channel(pipe, *_sig_rec),
				                                     Genode::env()->heap());
				Shared_pointer<Io_channel> pipe_source(new Pipe_source_io_channel(pipe, *_sig_rec),
				                                       Genode::env()->heap());

				_sysio->pipe_out.fd[0] = add_io_channel(pipe_source);
				_sysio->pipe_out.fd[1] = add_io_channel(pipe_sink);

				result = true;
				break;
			}

		case SYSCALL_DUP2:
			{
				int fd = add_io_channel(io_channel_by_fd(_sysio->dup2_in.fd),
				                        _sysio->dup2_in.to_fd);

				_sysio->dup2_out.fd = fd;

				result = true;
				break;
			}

		case SYSCALL_UNLINK:

			result = root_dir()->unlink(_sysio, _sysio->unlink_in.path);
			break;

		case SYSCALL_READLINK:

			result = root_dir()->readlink(_sysio, _sysio->readlink_in.path);
			break;


		case SYSCALL_RENAME:

			result = root_dir()->rename(_sysio, _sysio->rename_in.from_path,
			                                    _sysio->rename_in.to_path);
			break;

		case SYSCALL_MKDIR:

			result = root_dir()->mkdir(_sysio, _sysio->mkdir_in.path);
			break;

		case SYSCALL_SYMLINK:

			result = root_dir()->symlink(_sysio, _sysio->symlink_in.newpath);
			break;

		case SYSCALL_USERINFO:
			{
				if (_sysio->userinfo_in.request == Sysio::USERINFO_GET_UID
				    || _sysio->userinfo_in.request == Sysio::USERINFO_GET_GID) {
					_sysio->userinfo_out.uid = Noux::user_info()->uid;
					_sysio->userinfo_out.gid = Noux::user_info()->gid;

					result = true;
					break;
				}

				/*
				 * Since NOUX supports exactly one user, return false if we
				 * got a unknown uid.
				 */
				if (_sysio->userinfo_in.uid != Noux::user_info()->uid)
					break;

				Genode::memcpy(_sysio->userinfo_out.name,
				               Noux::user_info()->name,
				               sizeof(Noux::user_info()->name));
				Genode::memcpy(_sysio->userinfo_out.shell,
				               Noux::user_info()->shell,
				               sizeof(Noux::user_info()->shell));
				Genode::memcpy(_sysio->userinfo_out.home,
				               Noux::user_info()->home,
				               sizeof(Noux::user_info()->home));

				_sysio->userinfo_out.uid = user_info()->uid;
				_sysio->userinfo_out.gid = user_info()->gid;

				result = true;
				break;
			}

		case SYSCALL_GETTIMEOFDAY:
			{
				/**
				 * Since the timeout_scheduler thread is started after noux it
				 * basicly returns the eleapsed time since noux was started. We
				 * abuse this timer to provide a more useful implemenation of
				 * gettimeofday() to make certain programs (e.g. ping(1)) happy.
				 * Note: this is just a short-term solution because Genode currently
				 * lacks a proper time interface (there is a RTC driver however, but
				 * there is no interface for it).
				 */
				unsigned long time = Noux::timeout_scheduler()->curr_time();

				_sysio->gettimeofday_out.sec  = (time / 1000);
				_sysio->gettimeofday_out.usec = (time % 1000) * 1000;

				result = true;
				break;
			}

		case SYSCALL_CLOCK_GETTIME:
			{
				/**
				 * It's the same procedure as in SYSCALL_GETTIMEOFDAY.
				 */
				unsigned long time = Noux::timeout_scheduler()->curr_time();

				switch (_sysio->clock_gettime_in.clock_id) {

				/* CLOCK_SECOND is used by time(3) in the libc. */
				case Sysio::CLOCK_ID_SECOND:
					{
						_sysio->clock_gettime_out.sec    = (time / 1000);
						_sysio->clock_gettime_out.nsec   = 0;

						result = true;
						break;
					}

				default:
					{
						_sysio->clock_gettime_out.sec  = 0;
						_sysio->clock_gettime_out.nsec = 0;
						_sysio->error.clock            = Sysio::CLOCK_ERR_INVALID;

						break;
					}

				}

				break;

			}

		case SYSCALL_UTIMES:
			{
				/**
				 * This systemcall is currently not implemented because we lack
				 * the needed mechanisms in most file-systems.
				 *
				 * But we return true anyway to keep certain programs, e.g. make
				 * happy.
				 */
				result = true;
				break;
			}

		case SYSCALL_SYNC:
			{
				root_dir()->sync();
				result = true;
				break;
			}

		case SYSCALL_KILL:
			{
				if (_kill_broadcaster.kill(_sysio->kill_in.pid,
				                           _sysio->kill_in.sig))
					result = true;
				else
					_sysio->error.kill = Sysio::KILL_ERR_SRCH;

				break;
			}

		case SYSCALL_SOCKET:
		case SYSCALL_GETSOCKOPT:
		case SYSCALL_SETSOCKOPT:
		case SYSCALL_ACCEPT:
		case SYSCALL_BIND:
		case SYSCALL_LISTEN:
		case SYSCALL_SEND:
		case SYSCALL_SENDTO:
		case SYSCALL_RECV:
		case SYSCALL_RECVFROM:
		case SYSCALL_GETPEERNAME:
		case SYSCALL_SHUTDOWN:
		case SYSCALL_CONNECT:

			result = _syscall_net(sc);
			break;

		case SYSCALL_INVALID: break;
		}
	}

	catch (Invalid_fd) {
		_sysio->error.general = Sysio::ERR_FD_INVALID;
		PERR("Invalid file descriptor"); }

	catch (...) { PERR("Unexpected exception"); }

	/* handle signals which might have occured */
	while (!_pending_signals.empty() &&
		   (_sysio->pending_signals.avail_capacity() > 0)) {
		_sysio->pending_signals.add(_pending_signals.get());
	}

	return result;
}


/**
 * Return name of init process as specified in the config
 */
static char *name_of_init_process()
{
	enum { INIT_NAME_LEN = 128 };
	static char buf[INIT_NAME_LEN];
	Genode::config()->xml_node().sub_node("start").attribute("name").value(buf, sizeof(buf));
	return buf;
}


/**
 * Read command-line arguments of init process from config
 */
static Noux::Args const &args_of_init_process()
{
	static char args_buf[4096];
	static Noux::Args args(args_buf, sizeof(args_buf));

	Genode::Xml_node start_node = Genode::config()->xml_node().sub_node("start");

	try {
		/* the first argument is the program name */
		args.append(name_of_init_process());

		Genode::Xml_node arg_node = start_node.sub_node("arg");
		for (; ; arg_node = arg_node.next("arg")) {
			static char buf[512];
			arg_node.attribute("value").value(buf, sizeof(buf));
			args.append(buf);
		}
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }
	catch (Noux::Args::Overrun) { PERR("Argument buffer overrun"); }

	return args;
}


/**
 * Return string containing the environment variables of init
 *
 * The variable definitions are separated by zeros. The end of the string is
 * marked with another zero.
 */
static Noux::Sysio::Env &env_string_of_init_process()
{
	static Noux::Sysio::Env env;
	int index = 0;

	/* read environment variables for init process from config */
	Genode::Xml_node start_node = Genode::config()->xml_node().sub_node("start");
	try {
		Genode::Xml_node arg_node = start_node.sub_node("env");
		for (; ; arg_node = arg_node.next("env")) {
			static char name_buf[256], value_buf[256];

			arg_node.attribute("name").value(name_buf, sizeof(name_buf));
			arg_node.attribute("value").value(value_buf, sizeof(value_buf));

			Genode::size_t env_var_size = Genode::strlen(name_buf) +
			                              Genode::strlen("=") +
			                              Genode::strlen(value_buf) + 1;
			if (index + env_var_size < sizeof(env)) {
				Genode::snprintf(&env[index], env_var_size, "%s=%s", name_buf, value_buf);
				index += env_var_size;
			} else {
				env[index] = 0;
				break;
			}
		}
	}
	catch (Genode::Xml_node::Nonexistent_sub_node) { }

	return env;
}


Noux::Pid_allocator *Noux::pid_allocator()
{
	static Noux::Pid_allocator inst;
	return &inst;
}


Noux::Timeout_scheduler *Noux::timeout_scheduler()
{
	static Noux::Timeout_scheduler inst(0);
	return &inst;
}


Noux::User_info* Noux::user_info()
{
	static Noux::User_info inst;
	return &inst;
}


Noux::Io_receptor_registry * Noux::io_receptor_registry()
{
	static Noux::Io_receptor_registry _inst;
	return &_inst;
}


Terminal::Connection *Noux::terminal()
{
	static Terminal::Connection _inst;
	return &_inst;
}


Genode::Dataspace_capability Noux::ldso_ds_cap()
{
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		static Genode::Dataspace_capability ldso_ds = rom.dataspace();
		return ldso_ds;
	} catch (...) { }

	return Genode::Dataspace_capability();
}


/*
 * This lock is needed to delay the insertion of signals into a child object.
 * This is necessary during an 'execve()' syscall, when signals get copied from
 * the old child object to the new one. Without the lock, an IO channel could
 * insert a signal into both objects, which could lead to a duplicated signal
 * in the new child object.
 */
Genode::Lock &Noux::signal_lock()
{
	static Genode::Lock inst;
	return inst;
}


void *operator new (Genode::size_t size) {
	return Genode::env()->heap()->alloc(size); }


template <typename FILE_SYSTEM>
struct File_system_factory : Noux::File_system_registry::Entry
{
	Noux::File_system *create(Genode::Xml_node node) override
	{
		return new FILE_SYSTEM(node);
	}

	bool matches(Genode::Xml_node node) override
	{
		char buf[100];
		node.type_name(buf, sizeof(buf));
		return node.has_type(FILE_SYSTEM::name());
	}
};


int main(int argc, char **argv)
{
	using namespace Noux;
	PINF("--- noux started ---");

	/* register dynamic linker */
	Genode::Process::dynamic_linker(ldso_ds_cap());

	/* whitelist of service requests to be routed to the parent */
	static Genode::Service_registry parent_services;
	char const *service_names[] = { "LOG", "ROM", "Timer", 0 };
	for (unsigned i = 0; service_names[i]; i++)
		parent_services.insert(new Genode::Parent_service(service_names[i]));

	static Genode::Cap_connection cap;

	/* obtain global configuration */
	try {
		trace_syscalls = config()->xml_node().attribute("trace_syscalls").has_value("yes");
	} catch (Xml_node::Nonexistent_attribute) { }
	try {
		verbose = config()->xml_node().attribute("verbose").has_value("yes");
	} catch (Xml_node::Nonexistent_attribute) { }

	/* register file systems */
	static File_system_registry fs_registry;
	fs_registry.insert(*new File_system_factory<Tar_file_system>());
	fs_registry.insert(*new File_system_factory<Fs_file_system>());
	fs_registry.insert(*new File_system_factory<Terminal_file_system>());
	fs_registry.insert(*new File_system_factory<Null_file_system>());
	fs_registry.insert(*new File_system_factory<Zero_file_system>());
	fs_registry.insert(*new File_system_factory<Stdio_file_system>());
	fs_registry.insert(*new File_system_factory<Random_file_system>());
	fs_registry.insert(*new File_system_factory<Block_file_system>());

	/* initialize virtual file system */
	static Dir_file_system
		root_dir(config()->xml_node().sub_node("fstab"), fs_registry);

	/* set user information */
	try {
		user_info()->set_info(config()->xml_node().sub_node("user"));
	}
	catch (...) { }

	/* initialize network */
	init_network();

	/*
	 * Entrypoint used to virtualize child resources such as RAM, RM
	 */
	enum { STACK_SIZE = 2*1024*sizeof(long) };
	static Genode::Rpc_entrypoint resources_ep(&cap, STACK_SIZE, "noux_rsc_ep");

	/* create init process */
	static Genode::Signal_receiver sig_rec;
	static Destruct_queue destruct_queue;

	struct Kill_broadcaster_implementation : Kill_broadcaster
	{
		Family_member *init_process;

		bool kill(int pid, Noux::Sysio::Signal sig)
		{
			return init_process->deliver_kill(pid, sig);
		}
	};

	static Kill_broadcaster_implementation kill_broadcaster;

	init_child = new Noux::Child(name_of_init_process(),
	                             0,
	                             kill_broadcaster,
	                             *init_child,
	                             pid_allocator()->alloc(),
	                             &sig_rec,
	                             &root_dir,
	                             args_of_init_process(),
	                             env_string_of_init_process(),
	                             &cap,
	                             parent_services,
	                             resources_ep,
	                             false,
	                             env()->heap(),
	                             destruct_queue,
	                             verbose);

	kill_broadcaster.init_process = init_child;

	/*
	 * I/O channels must be dynamically allocated to handle cases where the
	 * init program closes one of these.
	 */
	typedef Terminal_io_channel Tio; /* just a local abbreviation */
	Shared_pointer<Io_channel>
		channel_0(new Tio(*Noux::terminal(), Tio::STDIN,  sig_rec), Genode::env()->heap()),
		channel_1(new Tio(*Noux::terminal(), Tio::STDOUT, sig_rec), Genode::env()->heap()),
		channel_2(new Tio(*Noux::terminal(), Tio::STDERR, sig_rec), Genode::env()->heap());

	init_child->add_io_channel(channel_0, 0);
	init_child->add_io_channel(channel_1, 1);
	init_child->add_io_channel(channel_2, 2);

	init_child->start();

	/* handle asynchronous events */
	while (init_child) {

		/*
		 * limit the scope of the 'Signal' object, so the signal context may
		 * get freed by the destruct queue
		 */
		{
			Genode::Signal signal = sig_rec.wait_for_signal();

			Signal_dispatcher_base *dispatcher =
				static_cast<Signal_dispatcher_base *>(signal.context());

			for (unsigned i = 0; i < signal.num(); i++)
				dispatcher->dispatch(1);
		}

		destruct_queue.flush();

		if (verbose_quota)
			PINF("quota: avail=%zd, used=%zd",
				 env()->ram_session()->avail(),
				 env()->ram_session()->used());
	}

	PINF("-- exiting noux ---");
	return 0;
}
