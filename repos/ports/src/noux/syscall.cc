/*
 * \brief  Noux syscall dispatcher
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Noux includes */
#include <child.h>
#include <child_env.h>
#include <vfs_io_channel.h>
#include <pipe_io_channel.h>

namespace Noux {

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


bool Noux::Child::syscall(Noux::Session::Syscall sc)
{
	if (_verbose.syscalls())
		log("PID ", pid(), " -> SYSCALL ", Noux::Session::syscall_name(sc));

	bool result = false;

	try {
		switch (sc) {

		case SYSCALL_WRITE:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.write_in.fd);

				if (!io->nonblocking())
					_block_for_io_channel(io, false, true, false);

				if (io->check_unblock(false, true, false)) {
					/* 'io->write' is expected to update '_sysio.write_out.count' */
					result = io->write(_sysio);
				} else
					_sysio.error.write = Vfs::File_io_service::WRITE_ERR_INTERRUPT;

				break;
			}

		case SYSCALL_READ:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.read_in.fd);

				if (!io->nonblocking())
					_block_for_io_channel(io, true, false, false);

				if (io->check_unblock(true, false, false))
					result = io->read(_sysio);
				else
					_sysio.error.read = Vfs::File_io_service::READ_ERR_INTERRUPT;

				break;
			}

		case SYSCALL_FTRUNCATE:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.ftruncate_in.fd);

				_block_for_io_channel(io, false, true, false);

				if (io->check_unblock(false, true, false))
					result = io->ftruncate(_sysio);
				else
					_sysio.error.ftruncate = Vfs::File_io_service::FTRUNCATE_ERR_INTERRUPT;

				break;
			}

		case SYSCALL_STAT:
		case SYSCALL_LSTAT: /* XXX implement difference between 'lstat' and 'stat' */
			{
				/**
				 * We calculate the inode by hashing the path because there is
				 * no inode registry in noux.
				 */
				size_t   path_len  = strlen(_sysio.stat_in.path);
				uint32_t path_hash = hash_path(_sysio.stat_in.path, path_len);

				Vfs::Directory_service::Stat stat_out;
				_sysio.error.stat = _root_dir.stat(_sysio.stat_in.path, stat_out);

				result = (_sysio.error.stat == Vfs::Directory_service::STAT_OK);

				/*
				 * Instead of using the uid/gid given by the actual file system
				 * we use the ones specificed in the config.
				 */
				if (result) {
					stat_out.uid = _user_info.uid();
					stat_out.gid = _user_info.gid();

					stat_out.inode = path_hash;
				}

				_sysio.stat_out.st = stat_out;

				break;
			}

		case SYSCALL_FSTAT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.fstat_in.fd);

				result = io->fstat(_sysio);

				if (result) {
					Sysio::Path path;

					/**
					 * Only actual fd's are valid fstat targets.
					 */
					if (io->path(path, sizeof (path))) {
						size_t path_len    = strlen(path);
						uint32_t path_hash = hash_path(path, path_len);

						_sysio.stat_out.st.inode = path_hash;
					}
				}

				break;
			}

		case SYSCALL_FCNTL:

			if (_sysio.fcntl_in.cmd == Sysio::FCNTL_CMD_SET_FD_FLAGS) {

				/* we assume that there is only the close-on-execve flag */
				close_fd_on_execve(_sysio.fcntl_in.fd, !!_sysio.fcntl_in.long_arg);
				result = true;
				break;
			}

			if (_sysio.fcntl_in.cmd == Sysio::FCNTL_CMD_GET_FD_FLAGS) {

				/* we assume that there is only the close-on-execve flag */
				_sysio.fcntl_out.result = close_fd_on_execve(_sysio.fcntl_in.fd);
				result = true;
				break;
			}

			result = _lookup_channel(_sysio.fcntl_in.fd)->fcntl(_sysio);
			break;

		case SYSCALL_OPEN:
			{
				Vfs::Vfs_handle *vfs_handle = 0;

				if (_root_dir.directory(_sysio.open_in.path)) {

					typedef Vfs::Directory_service::Opendir_result Opendir_result;
					typedef Vfs::Directory_service::Open_result Open_result;

					Opendir_result opendir_result =
						_root_dir.opendir(_sysio.open_in.path, false,
					                      &vfs_handle, _heap);

					switch (opendir_result) {
					case Opendir_result::OPENDIR_OK:
						_sysio.error.open = Open_result::OPEN_OK;
						break;
					case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:
						_sysio.error.open = Open_result::OPEN_ERR_UNACCESSIBLE;
						break;
					case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:
						_sysio.error.open = Open_result::OPEN_ERR_NAME_TOO_LONG;
						break;
					case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS:
						_sysio.error.open = Open_result::OPEN_ERR_EXISTS;
						break;
					case Opendir_result::OPENDIR_ERR_NO_SPACE:
						_sysio.error.open = Open_result::OPEN_ERR_NO_SPACE;
						break;
					case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:
					case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:
					case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:
						_sysio.error.open = Open_result::OPEN_ERR_NO_PERM;
						break;
					}

				} else {

					_sysio.error.open = _root_dir.open(_sysio.open_in.path,
					                                   _sysio.open_in.mode,
					                                   &vfs_handle, _heap);
				}

				if (!vfs_handle)
					break;

				char const *leaf_path = _root_dir.leaf_path(_sysio.open_in.path);

				/*
				 * File descriptors of opened directories are handled by
				 * '_root_dir'. In this case, we use the absolute path as leaf
				 * path because path operations always refer to the global
				 * root.
				 */
				if (&vfs_handle->ds() == &_root_dir)
					leaf_path = _sysio.open_in.path;

				Shared_pointer<Io_channel>
					channel(new (_heap) Vfs_io_channel(_sysio.open_in.path,
					                                   leaf_path, &_root_dir,
					                                   vfs_handle,
					                                   _vfs_io_waiter_registry,
					                                   _env.ep()),
					                                   _heap);

				_sysio.open_out.fd = add_io_channel(channel);
				result = true;
				break;
			}

		case SYSCALL_CLOSE:
			{
				remove_io_channel(_sysio.close_in.fd);
				result = true;
				break;
			}

		case SYSCALL_IOCTL:

			result = _lookup_channel(_sysio.ioctl_in.fd)->ioctl(_sysio);
			break;

		case SYSCALL_LSEEK:

			result = _lookup_channel(_sysio.lseek_in.fd)->lseek(_sysio);
			break;

		case SYSCALL_DIRENT:

			result = _lookup_channel(_sysio.dirent_in.fd)->dirent(_sysio);
			break;

		case SYSCALL_EXECVE:

			typedef Child_env<sizeof(_sysio.execve_in.args)> Execve_child_env;

			try {

				Execve_child_env child_env(_sysio.execve_in.filename,
					                       _sysio.execve_in.args,
					                       _sysio.execve_in.env,
					                       _root_dir, _vfs_io_waiter_registry,
					                       _env.ram(), _env.rm(), _heap);

				_parent_execve.execve_child(*this,
					                        child_env.binary_name(),
					                        child_env.args(),
					                        child_env.env());

				/*
				 * 'return' instead of 'break' to skip possible signal delivery,
				 * which might cause the old child process to exit itself
				 */
				return true;
			}
			catch (Execve_child_env::Binary_does_not_exist) {
				_sysio.error.execve = Sysio::EXECVE_ERR_NO_ENTRY; }
			catch (Execve_child_env::Binary_is_not_accessible) {
				_sysio.error.execve = Sysio::EXECVE_ERR_ACCESS; }
			catch (Execve_child_env::Binary_is_not_executable) {
				_sysio.error.execve = Sysio::EXECVE_ERR_NO_EXEC; }
			catch (Child::Insufficient_memory) {
				_sysio.error.execve = Sysio::EXECVE_ERR_NO_MEMORY; }

			break;

		case SYSCALL_SELECT:
			{
				size_t in_fds_total = _sysio.select_in.fds.total_fds();
				Sysio::Select_fds in_fds;
				for (Genode::size_t i = 0; i < in_fds_total; i++)
					in_fds.array[i] = _sysio.select_in.fds.array[i];
				in_fds.num_rd = _sysio.select_in.fds.num_rd;
				in_fds.num_wr = _sysio.select_in.fds.num_wr;
				in_fds.num_ex = _sysio.select_in.fds.num_ex;

				int _rd_array[in_fds_total];
				int _wr_array[in_fds_total];

				long timeout_sec     = _sysio.select_in.timeout.sec;
				long timeout_usec    = _sysio.select_in.timeout.usec;
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
							_sysio.select_out.fds.array[i] = _rd_array[i];
						}
						_sysio.select_out.fds.num_rd = unblock_rd;

						/* XXX could use a pointer to select_out.fds.array instead */
						for (size_t j = unblock_rd, i = 0; i < unblock_wr; i++, j++) {
							_sysio.select_out.fds.array[j] = _wr_array[i];
						}
						_sysio.select_out.fds.num_wr = unblock_wr;

						/* exception fds are currently not considered */
						_sysio.select_out.fds.num_ex = unblock_ex;

						result = true;
						break;
					}

					/*
					 * Return if timeout is zero or timeout exceeded
					 */

					if (_sysio.select_in.timeout.zero() || timeout_reached) {
						/*
						if (timeout_reached) log("timeout_reached");
						else                 log("timeout.zero()");
						*/
						_sysio.select_out.fds.num_rd = 0;
						_sysio.select_out.fds.num_wr = 0;
						_sysio.select_out.fds.num_ex = 0;

						result = true;
						break;
					}

					/*
					 * Return if signals are pending
					 */

					if (!_pending_signals.empty()) {
						_sysio.error.select = Sysio::SELECT_ERR_INTERRUPT;
						break;
					}

					/*
					 * Block at barrier except when reaching the timeout
					 */

					if (!_sysio.select_in.timeout.infinite()) {
						unsigned long to_msec = (timeout_sec * 1000) + (timeout_usec / 1000);
						Timeout_state ts;
						Timeout_alarm ta(ts, _blocker, _timeout_scheduler, to_msec);

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
				Genode::addr_t ip              = _sysio.fork_in.ip;
				Genode::addr_t sp              = _sysio.fork_in.sp;
				Genode::addr_t parent_cap_addr = _sysio.fork_in.parent_cap_addr;

				int const new_pid = _pid_allocator.alloc();
				Child * child = nullptr;

				try {
					/*
					 * XXX To ease debugging, it would be useful to generate a
					 *     unique name that includes the PID instead of just
					 *     reusing the name of the parent.
					 */
					child = new (_heap) Child(_child_policy.name(),
					                          _verbose,
					                          _user_info,
					                          this,
					                          _kill_broadcaster,
					                          _timeout_scheduler,
					                          *this,
					                          _pid_allocator,
					                          new_pid,
					                          _env,
					                          _root_dir,
					                          _vfs_io_waiter_registry,
					                          _args,
					                          _sysio_env.env(),
					                          _heap,
					                          _ref_pd, _ref_pd_cap,
					                          _parent_services,
					                          true,
					                          _destruct_queue);
				} catch (Child::Insufficient_memory) {
					_sysio.error.fork = Sysio::FORK_NOMEM;
					break;
				}

				Family_member::insert(child);

				_assign_io_channels_to(child, false);

				/* copy our address space into the new child */
				try {
					_pd.replay(child->pd(), _env.rm(), _heap,
					           child->ds_registry(), _ep);

					/* start executing the main thread of the new process */
					child->start_forked_main_thread(ip, sp, parent_cap_addr);

					/* activate child entrypoint, thereby starting the new process */
					child->start();

					_sysio.fork_out.pid = new_pid;

					result = true;
				}
				catch (Region_map::Region_conflict) {
					error("region conflict while replaying the address space"); }

				break;
			}

		case SYSCALL_GETPID:
			{
				_sysio.getpid_out.pid = pid();
				return true;
			}

		case SYSCALL_WAIT4:
			{
				Family_member *exited = _sysio.wait4_in.nohang ? poll4() : wait4();

				if (exited) {
					_sysio.wait4_out.pid    = exited->pid();
					_sysio.wait4_out.status = exited->exit_status();
					Family_member::remove(exited);

					static_cast<Child *>(exited)->submit_exit_signal();

				} else {
					if (_sysio.wait4_in.nohang) {
						_sysio.wait4_out.pid    = 0;
						_sysio.wait4_out.status = 0;
					} else {
						_sysio.error.wait4 = Sysio::WAIT4_ERR_INTERRUPT;
						break;
					}
				}
				result = true;
				break;
			}

		case SYSCALL_PIPE:
			{
				Shared_pointer<Pipe>       pipe       (new (_heap) Pipe,                                    _heap);
				Shared_pointer<Io_channel> pipe_sink  (new (_heap) Pipe_sink_io_channel  (pipe, _env.ep()), _heap);
				Shared_pointer<Io_channel> pipe_source(new (_heap) Pipe_source_io_channel(pipe, _env.ep()), _heap);

				_sysio.pipe_out.fd[0] = add_io_channel(pipe_source);
				_sysio.pipe_out.fd[1] = add_io_channel(pipe_sink);

				result = true;
				break;
			}

		case SYSCALL_DUP2:
			{
				int fd = add_io_channel(io_channel_by_fd(_sysio.dup2_in.fd),
				                        _sysio.dup2_in.to_fd);

				_sysio.dup2_out.fd = fd;

				result = true;
				break;
			}

		case SYSCALL_UNLINK:

			_sysio.error.unlink = _root_dir.unlink(_sysio.unlink_in.path);

			result = (_sysio.error.unlink == Vfs::Directory_service::UNLINK_OK);
			break;

		case SYSCALL_READLINK:
		{
			Vfs::Vfs_handle *symlink_handle { 0 };

			Vfs::Directory_service::Openlink_result openlink_result =
				_root_dir.openlink(_sysio.readlink_in.path, false, &symlink_handle, _heap);

			switch (openlink_result) {
			case Vfs::Directory_service::OPENLINK_OK:
				result = true;
				break;
			case Vfs::Directory_service::OPENLINK_ERR_LOOKUP_FAILED:
				_sysio.error.readlink = Sysio::READLINK_ERR_NO_ENTRY;
				break;
			case Vfs::Directory_service::OPENLINK_ERR_NAME_TOO_LONG:
			case Vfs::Directory_service::OPENLINK_ERR_NODE_ALREADY_EXISTS:
			case Vfs::Directory_service::OPENLINK_ERR_NO_SPACE:
			case Vfs::Directory_service::OPENLINK_ERR_OUT_OF_RAM:
			case Vfs::Directory_service::OPENLINK_ERR_OUT_OF_CAPS:
			case Vfs::Directory_service::OPENLINK_ERR_PERMISSION_DENIED:
				_sysio.error.readlink = Sysio::READLINK_ERR_NO_PERM;
			}

			if (openlink_result != Vfs::Directory_service::OPENLINK_OK)
				break;

			Vfs::file_size count = min(_sysio.readlink_in.bufsiz,
			                           sizeof(_sysio.readlink_out.chunk));

			Registered_no_delete<Vfs_io_waiter>
				vfs_io_waiter(_vfs_io_waiter_registry);

			while (!symlink_handle->fs().queue_read(symlink_handle, count))
				vfs_io_waiter.wait_for_io();

			Vfs_handle_context read_context;

			symlink_handle->context = &read_context;

			Vfs::file_size out_count = 0;
			Vfs::File_io_service::Read_result read_result;

			for (;;) {
				read_result = symlink_handle->fs().complete_read(symlink_handle,
				                                                 _sysio.readlink_out.chunk,
				                                                 count,
				                                                 out_count);

				if (read_result != Vfs::File_io_service::READ_QUEUED)
					break;

				read_context.vfs_io_waiter.wait_for_io();
			}

			/* wake up threads blocking for 'queue_*()' or 'write()' */
			_vfs_io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
				r.wakeup();
			});

			symlink_handle->ds().close(symlink_handle);

			_sysio.readlink_out.count = out_count;

			break;
		}

		case SYSCALL_RENAME:

			_sysio.error.rename = _root_dir.rename(_sysio.rename_in.from_path,
			                                       _sysio.rename_in.to_path);

			result = (_sysio.error.rename == Vfs::Directory_service::RENAME_OK);
			break;

		case SYSCALL_MKDIR:
		{
			Vfs::Vfs_handle *dir_handle { 0 };

			typedef Vfs::Directory_service::Opendir_result Opendir_result;

			Opendir_result opendir_result =
				_root_dir.opendir(_sysio.mkdir_in.path, true, &dir_handle, _heap);

			switch (opendir_result) {
			case Opendir_result::OPENDIR_OK:
				dir_handle->ds().close(dir_handle);
				result = true;
				break;
			case Opendir_result::OPENDIR_ERR_LOOKUP_FAILED:
				_sysio.error.mkdir = Sysio::MKDIR_ERR_NO_ENTRY;
				break;
			case Opendir_result::OPENDIR_ERR_NAME_TOO_LONG:
				_sysio.error.mkdir = Sysio::MKDIR_ERR_NAME_TOO_LONG;
				break;
			case Opendir_result::OPENDIR_ERR_NODE_ALREADY_EXISTS:
				_sysio.error.mkdir = Sysio::MKDIR_ERR_EXISTS;
				break;
			case Opendir_result::OPENDIR_ERR_NO_SPACE:
				_sysio.error.mkdir = Sysio::MKDIR_ERR_NO_SPACE;
				break;
			case Opendir_result::OPENDIR_ERR_OUT_OF_RAM:
			case Opendir_result::OPENDIR_ERR_OUT_OF_CAPS:
			case Opendir_result::OPENDIR_ERR_PERMISSION_DENIED:
				_sysio.error.mkdir = Sysio::MKDIR_ERR_NO_PERM;
				break;
			}

			break;
		}

		case SYSCALL_SYMLINK:
		{

			Vfs::Vfs_handle *symlink_handle { 0 };

			typedef Vfs::Directory_service::Openlink_result Openlink_result;

			Openlink_result openlink_result =
				_root_dir.openlink(_sysio.symlink_in.newpath, true,
				                   &symlink_handle, _heap);

			switch (openlink_result) {
			case Openlink_result::OPENLINK_OK:
				result = true;
				break;
			case Openlink_result::OPENLINK_ERR_LOOKUP_FAILED:
				_sysio.error.symlink = Sysio::SYMLINK_ERR_NO_ENTRY;
				break;
			case Openlink_result::OPENLINK_ERR_NAME_TOO_LONG:
			case Openlink_result::OPENLINK_ERR_NODE_ALREADY_EXISTS:
			case Openlink_result::OPENLINK_ERR_NO_SPACE:
			case Openlink_result::OPENLINK_ERR_OUT_OF_RAM:
			case Openlink_result::OPENLINK_ERR_OUT_OF_CAPS:
			case Openlink_result::OPENLINK_ERR_PERMISSION_DENIED:
				_sysio.error.symlink = Sysio::SYMLINK_ERR_NO_PERM;
			}

			if (openlink_result != Openlink_result::OPENLINK_OK)
				break;

			Vfs::file_size count = strlen(_sysio.symlink_in.oldpath) + 1;
			Vfs::file_size out_count;

			Registered_no_delete<Vfs_io_waiter>
				vfs_io_waiter(_vfs_io_waiter_registry);

			for (;;) {
				try {
					symlink_handle->fs().write(symlink_handle, _sysio.symlink_in.oldpath,
					                           strlen(_sysio.symlink_in.oldpath) + 1,
					                           out_count);
					break;
				} catch (Vfs::File_io_service::Insufficient_buffer) {
					vfs_io_waiter.wait_for_io();
				}
			}

			/* wake up threads blocking for 'queue_*()' or 'write()' */
			_vfs_io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
				r.wakeup();
			});

			if (out_count != count) {
				_sysio.error.symlink = Sysio::SYMLINK_ERR_NAME_TOO_LONG;
				result = false;
			}

			while (!symlink_handle->fs().queue_sync(symlink_handle))
				vfs_io_waiter.wait_for_io();

			Vfs_handle_context sync_context;

			symlink_handle->context = &sync_context;

			while (symlink_handle->fs().complete_sync(symlink_handle) ==
				   Vfs::File_io_service::SYNC_QUEUED)
				sync_context.vfs_io_waiter.wait_for_io();

			/* wake up threads blocking for 'queue_*()' or 'write()' */
			_vfs_io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
				r.wakeup();
			});

			symlink_handle->ds().close(symlink_handle);

			break;
		}

		case SYSCALL_USERINFO:
			{
				if (_sysio.userinfo_in.request == Sysio::USERINFO_GET_UID
				    || _sysio.userinfo_in.request == Sysio::USERINFO_GET_GID) {
					_sysio.userinfo_out.uid = _user_info.uid();
					_sysio.userinfo_out.gid = _user_info.gid();

					result = true;
					break;
				}

				/*
				 * Since NOUX supports exactly one user, return false if we
				 * got a unknown uid.
				 */
				if (_sysio.userinfo_in.uid != _user_info.uid())
					break;

				Genode::memcpy(_sysio.userinfo_out.name,
				               _user_info.name().string(),
				               sizeof(User_info::Name));
				Genode::memcpy(_sysio.userinfo_out.shell,
				               _user_info.shell().string(),
				               sizeof(User_info::Shell));
				Genode::memcpy(_sysio.userinfo_out.home,
				               _user_info.home().string(),
				               sizeof(User_info::Home));

				_sysio.userinfo_out.uid = _user_info.uid();
				_sysio.userinfo_out.gid = _user_info.gid();

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
				unsigned long time = _timeout_scheduler.curr_time();

				_sysio.gettimeofday_out.sec  = (time / 1000);
				_sysio.gettimeofday_out.usec = (time % 1000) * 1000;

				result = true;
				break;
			}

		case SYSCALL_CLOCK_GETTIME:
			{
				/**
				 * It's the same procedure as in SYSCALL_GETTIMEOFDAY.
				 */
				unsigned long time = _timeout_scheduler.curr_time();

				switch (_sysio.clock_gettime_in.clock_id) {

				/* CLOCK_SECOND is used by time(3) in the libc. */
				case Sysio::CLOCK_ID_SECOND:
					{
						_sysio.clock_gettime_out.sec    = (time / 1000);
						_sysio.clock_gettime_out.nsec   = 0;

						result = true;
						break;
					}

				default:
					{
						_sysio.clock_gettime_out.sec  = 0;
						_sysio.clock_gettime_out.nsec = 0;
						_sysio.error.clock            = Sysio::CLOCK_ERR_INVALID;

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
				/* no errors supported at this time */
				result = true;

				Vfs::Vfs_handle *sync_handle;

				Vfs::Directory_service::Opendir_result opendir_result =
					_root_dir.opendir("/", false, &sync_handle, _heap);

				if (opendir_result != Vfs::Directory_service::OPENDIR_OK)
					break;

				Registered_no_delete<Vfs_io_waiter>
					vfs_io_waiter(_vfs_io_waiter_registry);

				while (!sync_handle->fs().queue_sync(sync_handle))
					vfs_io_waiter.wait_for_io();

				Vfs_handle_context sync_context;

				sync_handle->context = &sync_context;

				while (sync_handle->fs().complete_sync(sync_handle) ==
				   Vfs::File_io_service::SYNC_QUEUED)
					sync_context.vfs_io_waiter.wait_for_io();

				/* wake up threads blocking for 'queue_*()' or 'write()' */
				_vfs_io_waiter_registry.for_each([] (Vfs_io_waiter &r) {
					r.wakeup();
				});

				sync_handle->ds().close(sync_handle);

				break;
			}

		case SYSCALL_KILL:
			{
				if (_kill_broadcaster.kill(_sysio.kill_in.pid,
				                           _sysio.kill_in.sig))
					result = true;
				else
					_sysio.error.kill = Sysio::KILL_ERR_SRCH;

				break;
			}

		case SYSCALL_GETDTABLESIZE:
			{
				_sysio.getdtablesize_out.n =
					Noux::File_descriptor_registry::MAX_FILE_DESCRIPTORS;
				result = true;
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
		_sysio.error.general = Vfs::Directory_service::ERR_FD_INVALID;
		error("invalid file descriptor"); }

	catch (...) { error("unexpected exception"); }

	/* handle signals which might have occured */
	while (!_pending_signals.empty() &&
		   (_sysio.pending_signals.avail_capacity() > 0)) {
		_sysio.pending_signals.add(_pending_signals.get());
	}

	return result;
}
