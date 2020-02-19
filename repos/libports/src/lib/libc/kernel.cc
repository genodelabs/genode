/*
 * \brief  Libc kernel for main and pthreads user contexts
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-01-22
 */

/*
 * Copyright (C) 2016-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc-internal includes */
#include <internal/kernel.h>

Libc::Kernel * Libc::Kernel::_kernel_ptr;


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


/**
 * Main context execution was suspended (on fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void suspended_callback()
{
	Libc::Kernel::kernel().entrypoint_suspended();
}


/**
 * Resume main context execution (after fork)
 *
 * This function is executed in the context of the initial thread.
 */
static void resumed_callback()
{
	Libc::Kernel::kernel().entrypoint_resumed();
}


size_t Libc::Kernel::_user_stack_size()
{
	size_t size = Component::stack_size();
	if (!_cloned)
		return size;

	_libc_env.libc_config().with_sub_node("stack", [&] (Xml_node stack) {
		size = stack.attribute_value("size", 0UL); });

	return size;
}


void Libc::Kernel::schedule_suspend(void(*original_suspended_callback) ())
{
	if (_state != USER) {
		error(__PRETTY_FUNCTION__, " called from non-user context");
		return;
	}

	/*
	 * We hook into suspend-resume callback chain to destruct and
	 * reconstruct parts of the kernel from the context of the initial
	 * thread, i.e., without holding any object locks.
	 */
	_original_suspended_callback = original_suspended_callback;
	_env.ep().schedule_suspend(suspended_callback, resumed_callback);

	if (!_setjmp(_user_context)) {
		_valid_user_context = true;
		_suspend_scheduled = true;
		_switch_to_kernel();
	} else {
		_valid_user_context = false;
	}
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
	auto init_fd = [&] (Xml_node const &node, char const *attr,
	                    int libc_fd, unsigned flags)
	{
		if (!node.has_attribute(attr))
			return;

		typedef String<Vfs::MAX_PATH_LEN> Path;
		Path const path = node.attribute_value(attr, Path());

		struct stat out_stat { };
		if (stat(path.string(), &out_stat) != 0)
			return;

		File_descriptor *fd = _vfs.open(path.string(), flags, libc_fd);
		if (!fd)
			return;

		if (fd->libc_fd != libc_fd) {
			error("could not allocate fd ",libc_fd," for ",path,", "
			      "got fd ",fd->libc_fd);
			_vfs.close(fd);
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
			char *dst = (char *)_heap.alloc(path.length());
			Genode::strncpy(dst, path.string(), path.length());
			fd->fd_path = dst;
		}

		::off_t const seek = node.attribute_value("seek", 0ULL);
		if (seek)
			_vfs.lseek(fd, seek, SEEK_SET);
	};

	if (_vfs.root_dir_has_dirents()) {

		Xml_node const node = _libc_env.libc_config();

		typedef String<Vfs::MAX_PATH_LEN> Path;

		if (node.has_attribute("cwd"))
			chdir(node.attribute_value("cwd", Path()).string());

		init_fd(node, "stdin",  0, O_RDONLY);
		init_fd(node, "stdout", 1, O_WRONLY);
		init_fd(node, "stderr", 2, O_WRONLY);

		node.for_each_sub_node("fd", [&] (Xml_node fd) {

			unsigned const id = fd.attribute_value("id", 0U);

			bool const rd = fd.attribute_value("readable",  false);
			bool const wr = fd.attribute_value("writeable", false);

			unsigned const flags = rd ? (wr ? O_RDWR : O_RDONLY)
			                          : (wr ? O_WRONLY : 0);

			if (!fd.has_attribute("path"))
				warning("Invalid <fd> node, 'path' attribute is missing");

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

	/* fetch user contex of the parent's application */
	_clone_connection->memory_content(&_user_context, sizeof(_user_context));
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


extern void (*libc_select_notify)();


void Libc::Kernel::handle_io_progress()
{
	/*
	 * TODO: make VFS I/O completion checks during
	 * kernel time to avoid flapping between stacks
	 */

	if (_io_ready) {
		_io_ready = false;

		/* some contexts may have been deblocked from select() */
		if (libc_select_notify)
			libc_select_notify();

		/*
		 * resume all as any VFS context may have
		 * been deblocked from blocking I/O
		 */
		Kernel::resume_all();
	}
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
	atexit(close_file_descriptors_on_exit);

	init_semaphore_support(_timer_accessor);
	init_pthread_support(*this, *this, _timer_accessor);

	_env.ep().register_io_progress_handler(*this);

	if (_cloned) {
		_clone_state_from_parent();

	} else {
		_malloc_heap.construct(*_malloc_ram, _env.rm());
		init_malloc(*_malloc_heap);
	}

	init_fork(_env, _libc_env, _heap, *_malloc_heap, _pid, *this, *this, _signal,
	          *this, _binary_name);
	init_execve(_env, _heap, _user_stack, *this, _binary_name,
	            *file_descriptor_allocator());
	init_plugin(*this);
	init_sleep(*this);
	init_vfs_plugin(*this);
	init_time(*this, _rtc_path, *this);
	init_select(*this, *this, *this, _signal);
	init_socket_fs(*this);
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
