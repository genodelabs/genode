/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc-internal includes */
#include <internal/kernel.h>

Libc::Kernel * Libc::Kernel::_kernel_ptr;


extern char **environ;


/**
 * Blockade for main context
 */

inline void Libc::Main_blockade::block()
{
	Check check { _woken_up };

	do {
		_timeout_ms = Kernel::kernel().suspend(check, _timeout_ms);
		_expired    = _timeout_valid && !_timeout_ms;
	} while (!woken_up() && !expired());
}

inline void Libc::Main_blockade::wakeup()
{
	_woken_up = true;
	Kernel::kernel().resume_main();
}


size_t Libc::Kernel::_user_stack_size()
{
	size_t size = Component::stack_size();

	_libc_env.libc_config().with_optional_sub_node("stack", [&] (Xml_node stack) {
		size = stack.attribute_value("size", Number_of_bytes(0)); });

	return size;
}


void Libc::Kernel::reset_malloc_heap()
{
	_malloc_ram.construct(_heap, _env.ram());

	_cloned_heap_ranges.for_each([&] (Registered<Cloned_malloc_heap_range> &r) {
		destroy(_heap, &r); });

	Heap &raw_malloc_heap = *_malloc_heap;
	construct_at<Heap>(&raw_malloc_heap, *_malloc_ram, _env.rm());

	reinit_malloc(raw_malloc_heap);
}


void Libc::Kernel::_init_file_descriptors()
{
	typedef Genode::Token<Vfs::Scanner_policy_path_element> Path_element_token;

	/* guard used to print an offending libc config when leaving the scope */
	struct Diag_guard
	{
		Kernel &kernel;
		bool show = false;

		Diag_guard(Kernel &kernel) : kernel(kernel) { }

		~Diag_guard() { if (show) log(kernel._libc_env.libc_config()); }

	} diag_guard { *this };

	auto resolve_symlinks = [&] (Absolute_path next_iteration_working_path, Absolute_path &resolved_path)
	{
		char path_element[PATH_MAX];
		char symlink_target[PATH_MAX];

		Absolute_path current_iteration_working_path;

		enum { FOLLOW_LIMIT = 10 };
		int follow_count = 0;
		bool symlink_resolved_in_this_iteration;
		do {
			if (follow_count++ == FOLLOW_LIMIT) {
				errno = ELOOP;
				throw Symlink_resolve_error();
			}

			current_iteration_working_path = next_iteration_working_path;

			next_iteration_working_path.import("");
			symlink_resolved_in_this_iteration = false;

			Path_element_token t(current_iteration_working_path.base());

			while (t) {
				if (t.type() != Path_element_token::IDENT) {
						t = t.next();
						continue;
				}

				t.string(path_element, sizeof(path_element));

				try {
					next_iteration_working_path.append_element(path_element);
				} catch (Path_base::Path_too_long) {
					errno = ENAMETOOLONG;
					throw Symlink_resolve_error();
				}

				/*
				 * If a symlink has been resolved in this iteration, the remaining
				 * path elements get added and a new iteration starts.
				 */
				if (!symlink_resolved_in_this_iteration) {
					struct stat stat_buf;
					int res = _vfs.stat_from_kernel(next_iteration_working_path.base(), &stat_buf);
					if (res == -1) {
						throw Symlink_resolve_error();
					}
					if (S_ISLNK(stat_buf.st_mode)) {
						res = readlink(next_iteration_working_path.base(),
						               symlink_target, sizeof(symlink_target) - 1);
						if (res < 1)
							throw Symlink_resolve_error();

						/* zero terminate target */
						symlink_target[res] = 0;

						if (symlink_target[0] == '/')
							/* absolute target */
							next_iteration_working_path.import(symlink_target, _cwd.base());
						else {
							/* relative target */
							next_iteration_working_path.strip_last_element();
							try {
								next_iteration_working_path.append_element(symlink_target);
							} catch (Path_base::Path_too_long) {
								errno = ENAMETOOLONG;
								throw Symlink_resolve_error();
							}
						}
						symlink_resolved_in_this_iteration = true;
					}
				}

				t = t.next();
			}

		} while (symlink_resolved_in_this_iteration);

		resolved_path = next_iteration_working_path;
		resolved_path.remove_trailing('/');
	};

	typedef String<Vfs::MAX_PATH_LEN> Path;

	auto resolve_absolute_path = [&] (Path const &path)
	{
		Absolute_path abs_path { };
		Absolute_path abs_dir(path.string(), _cwd.base());   abs_dir.strip_last_element();
		Absolute_path dir_entry(path.string(), _cwd.base()); dir_entry.keep_only_last_element();

		try {
			resolve_symlinks(abs_dir, abs_path);
			abs_path.append_element(dir_entry.string());
			return abs_path;
		} catch (Path_base::Path_too_long) { return Absolute_path(); }
	};

	auto init_fd = [&] (Xml_node const &node, char const *attr,
	                    int libc_fd, unsigned flags)
	{
		if (!node.has_attribute(attr))
			return;

		Path const attr_value { node.attribute_value(attr, Path()) };
		try {
			Absolute_path const path { resolve_absolute_path(attr_value) };

			struct stat out_stat { };
			if (_vfs.stat_from_kernel(path.string(), &out_stat) != 0) {
				warning("failed to call 'stat' on ", path);
				diag_guard.show = true;
				return;
			}

			File_descriptor *fd =
				_vfs.open_from_kernel(path.string(), flags, libc_fd);

			if (!fd)
				return;

			if (fd->libc_fd != libc_fd) {
				error("could not allocate fd ",libc_fd," for ",path,", "
			          "got fd ",fd->libc_fd);
				_vfs.close_from_kernel(fd);
				diag_guard.show = true;
				return;
			}

			fd->cloexec = node.attribute_value("cloexec", false);

			/*
			 * We need to manually register the path. Normally this is done
			 * by '_open'. But we call the local 'open' function directly
			 * because we want to explicitly specify the libc fd ID.
			 */
			if (fd->fd_path)
				warning("may leak former FD path memory");

			{
				char *dst = (char *)_heap.alloc(path.max_len());
				copy_cstring(dst, path.string(), path.max_len());
				fd->fd_path = dst;
			}

			::off_t const seek = node.attribute_value("seek", 0ULL);
			if (seek)
				_vfs.lseek_from_kernel(fd, seek);

		} catch (Symlink_resolve_error) {
			warning("failed to resolve path for ", attr_value);
			diag_guard.show = true;
			return;
		}
	};

	if (_vfs.root_dir_has_dirents()) {

		Xml_node const node = _libc_env.libc_config();

		using Path = String<Vfs::MAX_PATH_LEN>;

		if (node.has_attribute("cwd"))
			_cwd.import(node.attribute_value("cwd", Path()).string(), _cwd.base());

		init_fd(node, "stdin",  0, O_RDONLY);
		init_fd(node, "stdout", 1, O_WRONLY);
		init_fd(node, "stderr", 2, O_WRONLY);

		node.for_each_sub_node("fd", [&] (Xml_node fd) {

			unsigned const id = fd.attribute_value("id", 0U);

			bool const rd = fd.attribute_value("readable",  false);
			bool const wr = fd.attribute_value("writeable", false);

			unsigned const flags = rd ? (wr ? O_RDWR : O_RDONLY)
			                          : (wr ? O_WRONLY : 0);

			if (!fd.has_attribute("path")) {
				warning("unknown path for file descriptor ", id);
				diag_guard.show = true;
			}

			init_fd(fd, "path", id, flags);
		});

		/* prevent use of IDs of stdin, stdout, and stderr for other files */
		for (unsigned fd = 0; fd <= 2; fd++)
			file_descriptor_allocator()->preserve(fd);
	}

	/**
	 * Call 'fn' with root directory and path to ioctl pseudo file as arguments
	 *
	 * If no matching ioctl pseudo file exists, 'fn' is not called.
	 */
	auto with_ioctl_path = [&] (File_descriptor const *fd, char const *file, auto fn)
	{
		if (!fd || !fd->fd_path)
			return;

		Absolute_path const ioctl_dir = Vfs_plugin::ioctl_dir(*fd);
		Absolute_path path = ioctl_dir;
		path.append_element(file);

		_vfs.with_root_dir([&] (Directory &root_dir) {
			if (root_dir.file_exists(path.string()))
				fn(root_dir, path.string()); });
	};

	/*
	 * Watch stdout's 'info' pseudo file to detect terminal-resize events
	 */
	File_descriptor const * const stdout_fd =
		file_descriptor_allocator()->find_by_libc_fd(STDOUT_FILENO);

	with_ioctl_path(stdout_fd, "info", [&] (Directory &root_dir, char const *path) {
		_terminal_resize_handler.construct(root_dir, path, *this,
		                                   &Kernel::_handle_terminal_resize); });

	/*
	 * Watch stdin's 'interrupts' pseudo file to detect control-c events
	 */
	File_descriptor const * const stdin_fd =
		file_descriptor_allocator()->find_by_libc_fd(STDIN_FILENO);

	with_ioctl_path(stdin_fd, "interrupts", [&] (Directory &root_dir, char const *path) {
		_user_interrupt_handler.construct(root_dir, path,
		                                  *this, &Kernel::_handle_user_interrupt); });
}


void Libc::Kernel::_handle_terminal_resize()
{
	_signal.charge(SIGWINCH);
	_resume_main();
}


void Libc::Kernel::_handle_user_interrupt()
{
	_signal.charge(SIGINT);
	_resume_main();
}


void Libc::Kernel::_clone_state_from_parent()
{
	struct Range { void *at; size_t size; };

	auto range_attr = [&] (Xml_node node)
	{
		return Range {
			.at   = (void *)node.attribute_value("at",   0UL),
			.size =         node.attribute_value("size", 0UL)
		};
	};

	/*
	 * Allocate local memory for the backing store of the application heap,
	 * mirrored from the parent.
	 *
	 * This step must precede the creation of the 'Clone_connection' because
	 * the shared-memory buffer of the clone session may otherwise potentially
	 * interfere with such a heap region.
	 */
	_libc_env.libc_config().for_each_sub_node("heap", [&] (Xml_node node) {
		Range const range = range_attr(node);
		new (_heap)
			Registered<Cloned_malloc_heap_range>(_cloned_heap_ranges,
			                                     _env.ram(), _env.rm(),
			                                     range.at, range.size); });

	_clone_connection.construct(_env);

	/* fetch heap content */
	_cloned_heap_ranges.for_each([&] (Cloned_malloc_heap_range &heap_range) {
		heap_range.import_content(*_clone_connection); });

	/* value of global environ pointer (the env vars are already on the heap) */
	_clone_connection->memory_content(&environ, sizeof(environ));

	/* fetch user contex of the parent's application */
	_clone_connection->memory_content(&_user_context, sizeof(_user_context));
	_clone_connection->memory_content(&_main_monitor_job, sizeof(_main_monitor_job));
	_valid_user_context = true;

	_libc_env.libc_config().for_each_sub_node([&] (Xml_node node) {

		auto copy_from_parent = [&] (Range range)
		{
			_clone_connection->memory_content(range.at, range.size);
		};

		/* clone application stack */
		if (node.type() == "stack")
			copy_from_parent(range_attr(node));

		/* clone RW segment of a shared library or the binary */
		if (node.type() == "rw") {
			typedef String<64> Name;
			Name const name = node.attribute_value("name", Name());

			/*
			 * The blacklisted segments are initialized via the
			 * regular startup of the child.
			 */
			bool const blacklisted = (name == "ld.lib.so")
			                      || (name == "libc.lib.so")
			                      || (name == "libm.lib.so")
			                      || (name == "posix.lib.so")
			                      || (strcmp(name.string(), "vfs", 3) == 0);
			if (!blacklisted)
				copy_from_parent(range_attr(node));
		}
	});

	/* import application-heap state from parent */
	_clone_connection->object_content(_malloc_heap);
	init_malloc_cloned(*_clone_connection);
}


void Libc::Kernel::handle_io_progress()
{
	if (_io_progressed) {
		_io_progressed = false;

		Kernel::resume_all();

		if (_execute_monitors_pending == Monitor::Pool::State::JOBS_PENDING)
			_execute_monitors_pending = _monitors.execute_monitors();
	}

	wakeup_remote_peers();
}


void Libc::execute_in_application_context(Application_code &app_code)
{
	/*
	 * The libc execution model builds on the main entrypoint, which handles
	 * all relevant signals (e.g., timing and VFS). Additional component
	 * entrypoints or pthreads should never call with_libc() but we catch this
	 * here and just execute the application code directly.
	 */
	if (!Kernel::kernel().main_context()) {
		app_code.execute();
		Kernel::kernel().wakeup_remote_peers();
		return;
	}

	static bool nested = false;

	if (nested) {

		if (Kernel::kernel().main_suspended()) {
			Kernel::kernel().nested_execution(app_code);
		} else {
			app_code.execute();
		}
		return;
	}

	nested = true;
	Kernel::kernel().run(app_code);
	nested = false;

	Kernel::kernel().wakeup_remote_peers();
}


static void close_file_descriptors_on_exit()
{
	for (;;) {
		int const fd = Libc::file_descriptor_allocator()->any_open_fd();
		if (fd == -1)
			break;
		close(fd);
	}
}


Libc::Kernel::Kernel(Genode::Env &env, Genode::Allocator &heap)
:
	_env(env), _heap(heap)
{
	init_atexit(_atexit);

	atexit(close_file_descriptors_on_exit);

	init_semaphore_support(_timer_accessor);
	init_pthread_support(*this, _timer_accessor);
	init_pthread_support(env.cpu(), _pthread_config(), _heap);

	_env.ep().register_io_progress_handler(*this);

	if (_cloned) {
		_clone_state_from_parent();

	} else {
		_malloc_heap.construct(*_malloc_ram, _env.rm());
		init_malloc(*_malloc_heap);
	}

	init_fork(_env, _libc_env, _heap, *_malloc_heap, _pid, *this, _signal,
	          _binary_name);
	init_execve(_env, _heap, _user_stack, *this, _binary_name,
	            *file_descriptor_allocator());
	init_plugin(*this);
	init_sleep(*this);
	init_vfs_plugin(*this, _env.rm());
	init_file_operations(*this, _libc_env);
	init_time(*this, *this);
	init_poll(_signal, *this);
	init_select(*this);
	init_socket_fs(*this, *this);
	init_passwd(_passwd_config());
	init_signal(_signal);

	_init_file_descriptors();

	_kernel_ptr = this;

	/*
	 * Acknowledge the completion of 'fork' to the parent
	 *
	 * This must be done after '_init_file_descriptors' to ensure that pipe FDs
	 * of the parent are opened at the child before the parent continues.
	 * Otherwise, the parent would potentially proceed with closing the pipe
	 * FDs before the child has a chance to open them. In this situation, the
	 * pipe reference counter may reach an intermediate value of zero,
	 * triggering the destruction of the pipe.
	 */
	if (_cloned)
		_clone_connection.destruct();
}
