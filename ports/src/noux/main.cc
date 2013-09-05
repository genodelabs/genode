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
			Timeout_scheduler(unsigned long curr_time) : _curr_time(curr_time) { start(); }

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
			Semaphore         *_blocker;
			Timeout_scheduler *_scheduler;

		public:
			Timeout_alarm(Timeout_state *st, Semaphore *blocker, Timeout_scheduler *scheduler, Time timeout)
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
				_blocker->up();

				return false;
			}
	};
};


/*****************************
 ** Noux syscall dispatcher **
 *****************************/

bool Noux::Child::syscall(Noux::Session::Syscall sc)
{
	if (trace_syscalls)
		Genode::printf("PID %d -> SYSCALL %s\n",
		               pid(), Noux::Session::syscall_name(sc));

	try {
		switch (sc) {

		case SYSCALL_WRITE:
			{
				size_t const count_in = _sysio->write_in.count;

				for (size_t count = 0; count != count_in; ) {

					Shared_pointer<Io_channel> io = _lookup_channel(_sysio->write_in.fd);

					if (!io->is_nonblocking())
						if (!io->check_unblock(false, true, false))
							_block_for_io_channel(io);

					/*
					 * 'io->write' is expected to update 'write_out.count'
					 */
					if (io->write(_sysio, count) == false)
						return false;
				}

				return true;
			}

		case SYSCALL_READ:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio->read_in.fd);

				if (!io->is_nonblocking())
					while (!io->check_unblock(true, false, false))
						_block_for_io_channel(io);

				return io->read(_sysio);
			}

		case SYSCALL_FTRUNCATE:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio->ftruncate_in.fd);

				while (!io->check_unblock(true, false, false))
					_block_for_io_channel(io);

				return io->ftruncate(_sysio);
			}

		case SYSCALL_STAT:
		case SYSCALL_LSTAT: /* XXX implement difference between 'lstat' and 'stat' */
			{
				bool result = root_dir()->stat(_sysio, _sysio->stat_in.path);

				/**
 				 * Instead of using the uid/gid given by the actual file system
				 * we use the ones specificed in the config.
				 */
				if (result) {
					_sysio->stat_out.st.uid = user_info()->uid;
					_sysio->stat_out.st.gid = user_info()->gid;
				}

				return result;
			}

		case SYSCALL_FSTAT:

			return _lookup_channel(_sysio->fstat_in.fd)->fstat(_sysio);

		case SYSCALL_FCNTL:

			if (_sysio->fcntl_in.cmd == Sysio::FCNTL_CMD_SET_FD_FLAGS) {

				/* we assume that there is only the close-on-execve flag */
				_lookup_channel(_sysio->fcntl_in.fd)->close_on_execve =
					!!_sysio->fcntl_in.long_arg;
				return true;
			}

			return _lookup_channel(_sysio->fcntl_in.fd)->fcntl(_sysio);

		case SYSCALL_OPEN:
			{
				Vfs_handle *vfs_handle = root_dir()->open(_sysio, _sysio->open_in.path);
				if (!vfs_handle)
					return false;

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
					                           leaf_path, root_dir(), vfs_handle),
					        Genode::env()->heap());

				_sysio->open_out.fd = add_io_channel(channel);
				return true;
			}

		case SYSCALL_CLOSE:
			{
				remove_io_channel(_sysio->close_in.fd);
				return true;
			}

		case SYSCALL_IOCTL:

			return _lookup_channel(_sysio->ioctl_in.fd)->ioctl(_sysio);

		case SYSCALL_LSEEK:

			return _lookup_channel(_sysio->lseek_in.fd)->lseek(_sysio);

		case SYSCALL_DIRENT:

			return _lookup_channel(_sysio->dirent_in.fd)->dirent(_sysio);

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
					return false;
				}

				Child_env<sizeof(_sysio->execve_in.args)>
					child_env(_sysio->execve_in.filename, binary_ds,
					          _sysio->execve_in.args, _sysio->execve_in.env);

				root_dir()->release(_sysio->execve_in.filename, binary_ds);

				binary_ds = root_dir()->dataspace(child_env.binary_name());

				if (!binary_ds.valid()) {
					_sysio->error.execve = Sysio::EXECVE_NONEXISTENT;
					return false;
				}

				root_dir()->release(child_env.binary_name(), binary_ds);

				try {
					Child *child = new Child(child_env.binary_name(),
					                         parent(),
					                         pid(),
					                         _sig_rec,
					                         root_dir(),
					                         child_env.args(),
					                         child_env.env(),
					                         _cap_session,
					                         _parent_services,
					                         _resources.ep,
					                         false,
					                         env()->heap(),
					                         _destruct_queue,
					                         verbose);

					/* replace ourself by the new child at the parent */
					parent()->remove(this);
					parent()->insert(child);

					_assign_io_channels_to(child);

					/* signal main thread to remove ourself */
					Genode::Signal_transmitter(_destruct_context_cap).submit();

					/* start executing the new process */
					child->start();

					/* this child will be removed by the execve_finalization_dispatcher */
					return true;
				}
				catch (Child::Binary_does_not_exist) {
					_sysio->error.execve = Sysio::EXECVE_NONEXISTENT; }

				return false;
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

						return true;
					}

					/*
					 * Return if I/O channel triggered, but timeout exceeded
					 */
					
					if (_sysio->select_in.timeout.zero() || timeout_reached) {
						/*
						if (timeout_reached) PINF("timeout_reached");
						else                 PINF("timeout.zero()");
						*/
						_sysio->select_out.fds.num_rd = 0;
						_sysio->select_out.fds.num_wr = 0;
						_sysio->select_out.fds.num_ex = 0;

						return true;
					}

					/*
					 * Register ourself at all watched I/O channels
					 *
					 * We instantiate as many notifiers as we have file
					 * descriptors to observe. Each notifier is associated
					 * with the child's blocking semaphore. When any of the
					 * notifiers get woken up, the semaphore gets unblocked.
					 *
					 * XXX However, the semaphore may get unblocked for other
					 *     conditions such as the destruction of the child.
					 *     ...to be done.
					 */

					Wake_up_notifier notifiers[in_fds_total];

					for (Genode::size_t i = 0; i < in_fds_total; i++) {
						int fd = in_fds.array[i];
						if (!fd_in_use(fd)) continue;

						Shared_pointer<Io_channel> io = io_channel_by_fd(fd);
						notifiers[i].semaphore = &_blocker;
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
					 * Block at barrier except when reaching the timeout
					 */

					if (!_sysio->select_in.timeout.infinite()) {
						unsigned long to_msec = (timeout_sec * 1000) + (timeout_usec / 1000);
						Timeout_state ts;
						Timeout_alarm ta(&ts, &_blocker, Noux::timeout_scheduler(), to_msec);

						/* block until timeout is reached or we were unblocked */
						_blocker.down();

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
						_blocker.down();
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

				}

				return true;
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

				return true;
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
					_sysio->wait4_out.pid    = 0;
					_sysio->wait4_out.status = 0;
				}
				return true;
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

				return true;
			}

		case SYSCALL_DUP2:
			{
				int fd = add_io_channel(io_channel_by_fd(_sysio->dup2_in.fd),
				                        _sysio->dup2_in.to_fd);

				_sysio->dup2_out.fd = fd;

				return true;
			}

		case SYSCALL_UNLINK:

			return root_dir()->unlink(_sysio, _sysio->unlink_in.path);

		case SYSCALL_READLINK:

			return root_dir()->readlink(_sysio, _sysio->readlink_in.path);


		case SYSCALL_RENAME:

			return root_dir()->rename(_sysio, _sysio->rename_in.from_path,
			                                 _sysio->rename_in.to_path);

		case SYSCALL_MKDIR:

			return root_dir()->mkdir(_sysio, _sysio->mkdir_in.path);

		case SYSCALL_SYMLINK:

			return root_dir()->symlink(_sysio, _sysio->symlink_in.newpath);

		case SYSCALL_USERINFO:
			{
				if (_sysio->userinfo_in.request == Sysio::USERINFO_GET_UID
				    || _sysio->userinfo_in.request == Sysio::USERINFO_GET_GID) {
					_sysio->userinfo_out.uid = Noux::user_info()->uid;
					_sysio->userinfo_out.gid = Noux::user_info()->gid;

					return true;
				}

				/*
				 * Since NOUX supports exactly one user, return false if we
				 * got a unknown uid.
				 */
				if (_sysio->userinfo_in.uid != Noux::user_info()->uid)
					return false;

				Genode::memcpy(_sysio->userinfo_out.name,  Noux::user_info()->name,  sizeof(Noux::user_info()->name));
				Genode::memcpy(_sysio->userinfo_out.shell, Noux::user_info()->shell, sizeof(Noux::user_info()->shell));
				Genode::memcpy(_sysio->userinfo_out.home,  Noux::user_info()->home,  sizeof(Noux::user_info()->home));

				_sysio->userinfo_out.uid = user_info()->uid;
				_sysio->userinfo_out.gid = user_info()->gid;

				return true;
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

				return true;
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

						return true;
					}

				default:
					{
						_sysio->clock_gettime_out.sec  = 0;
						_sysio->clock_gettime_out.nsec = 0;
						_sysio->error.clock            = Sysio::CLOCK_ERR_INVALID;

						return false;
					}

				}

				return false;

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
				return true;
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

			return _syscall_net(sc);

		case SYSCALL_INVALID: break;
		}
	}

	catch (Invalid_fd) {
		_sysio->error.general = Sysio::ERR_FD_INVALID;
		PERR("Invalid file descriptor"); }

	catch (...) { PERR("Unexpected exception"); }

	return false;
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


void *operator new (Genode::size_t size) {
	return Genode::env()->heap()->alloc(size); }


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

	/* initialize virtual file system */
	static Dir_file_system
		root_dir(config()->xml_node().sub_node("fstab"));

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

	init_child = new Noux::Child(name_of_init_process(),
	                             0,
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
