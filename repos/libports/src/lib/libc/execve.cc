/*
 * \brief  Libc execve mechanism
 * \author Norman Feske
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/shared_object.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>

/* libc includes */
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <libc/allocator.h>
#include <libc-plugin/fd_alloc.h>

/* libc-internal includes */
#include <internal/call_func.h>
#include <internal/init.h>
#include <internal/errno.h>
#include <internal/file_operations.h>

using namespace Genode;

typedef int (*main_fn_ptr) (int, char **, char **);

namespace Libc {
	struct Interpreter;
	struct String_array;
}


struct Libc::Interpreter
{
	Attached_rom_dataspace _rom;

	char **args = { nullptr };

	/**
	 * Return true if file content starts with the specified C string
	 */
	bool _content_starts_with(char const *prefix) const
	{
		size_t const prefix_len = ::strlen(prefix);

		if (prefix_len > _rom.size())
			return false;

		return strncmp(prefix, _rom.local_addr<char const>(), prefix_len) == 0;
	}

	bool script()         const { return _content_starts_with("#!"); }
	bool elf_executable() const { return _content_starts_with("\x7f" "ELF"); }

	struct Arg
	{
		/* pointer to first character */
		char const * const ptr;

		/* number of characters */
		size_t const length;
	};

	struct Constrained_ptr
	{
		char const *ptr;

		size_t remaining_bytes;

		/**
		 * Skip characters for which 'condition' is true
		 */
		template <typename COND>
		void _skip(COND const &condition)
		{
			while (remaining_bytes > 0 && condition(*ptr)) {
				ptr++;
				remaining_bytes--;
			}
		}

		void skip_non_whitespace() { _skip([] (char c) { return !is_whitespace(c); }); };
		void skip_whitespace()     { _skip([] (char c) { return  is_whitespace(c); }); };

		void skip_shebang()
		{
			_skip([] (char c) { return c == '#'; });
			_skip([] (char c) { return c == '!'; });
		}

		bool valid() const { return ptr && *ptr && *ptr != '\n' && remaining_bytes; }
	};

	template <typename FN>
	void _for_each_arg(FN const &fn) const
	{
		if (!script())
			return;

		Constrained_ptr ptr { _rom.local_addr<char const>(), _rom.size() };

		ptr.skip_shebang();

		/* skip whitespace between shebang and interpreter */
		ptr.skip_whitespace();

		/* skip path to interpreter */
		ptr.skip_non_whitespace();

		while (ptr.valid()) {

			/* skip whitespace before argument */
			ptr.skip_whitespace();

			/* find end of argument */
			char const * const arg_ptr = ptr.ptr;
			unsigned const orig_remaining_bytes = ptr.remaining_bytes;
			ptr.skip_non_whitespace();

			size_t const length = orig_remaining_bytes - ptr.remaining_bytes;

			if (length)
				fn(Arg { arg_ptr, length });
		}
	}

	typedef String<Vfs::MAX_PATH_LEN> Path;

	Path path() const
	{
		if (!script())
			return Path();

		Constrained_ptr ptr { _rom.local_addr<char const>(), _rom.size() };

		ptr.skip_shebang();
		ptr.skip_whitespace();

		/* find end of interpreter path */
		char const * const path_ptr = ptr.ptr;
		unsigned const orig_remaining_bytes = ptr.remaining_bytes;
		ptr.skip_non_whitespace();

		size_t const length = orig_remaining_bytes - ptr.remaining_bytes;

		return Path(Cstring(path_ptr, length));
	}

	unsigned _count_args() const
	{
		unsigned count = 0;

		_for_each_arg([&] (Arg) { count++; });

		return count;
	}

	unsigned const num_args;

	Interpreter(Genode::Env &env, char const * const filename)
	:
		_rom(env, filename), num_args(_count_args() + 2 /* argv0 + filename */)
	{
		if (script()) {
			args = (char **)calloc(num_args + 1 /* null termination */, sizeof(char *));

			unsigned i = 0;

			/* argv0 */
			args[i++] = strdup(path().string());

			_for_each_arg([&] (Arg arg) {
				args[i++] = strndup(arg.ptr, arg.length); });

			/* supply script file name as last argument */
			args[i++] = strdup(filename);
		}
	}

	~Interpreter()
	{
		if (script()) {
			for (unsigned i = 0; i < num_args; i++)
				free(args[i]);

			free(args);
		}
	}
};


/**
 * Utility to capture the state of argv or envp string arrays
 */
struct Libc::String_array : Noncopyable
{
	typedef Genode::Allocator Allocator;

	Allocator &_alloc;

	static unsigned _num_entries(char const * const * const array)
	{
		unsigned i = 0;
		for (; array && array[i]; i++);
		return i;
	}

	unsigned const count;

	size_t const _array_bytes = sizeof(char *)*(count + 1);

	char ** const array = (char **)_alloc.alloc(_array_bytes);

	struct Buffer
	{
		Allocator &_alloc;
		size_t const _size;
		char * const _base = (char *)_alloc.alloc(_size);

		unsigned _pos = 0;

		Buffer(Allocator &alloc, size_t size)
		: _alloc(alloc), _size(size) { }

		~Buffer() { _alloc.free(_base, _size); }

		bool try_append(char const *s)
		{
			size_t const len = ::strlen(s) + 1;
			if (_pos + len > _size)
				return false;

			Genode::strncpy(_base + _pos, s, len);
			_pos += len;
			return true;
		}

		char *pos_ptr() { return _base + _pos; }
	};

	Constructible<Buffer> _buffer { };

	String_array(Allocator &alloc,
	             char const * const * const src_array_1,
	             char const * const * const src_array_2 = nullptr)
	:
		_alloc(alloc),

		/* if 'src_array_2' is supplied, we skip its first element (argv0) */
		count(_num_entries(src_array_1) + _num_entries(src_array_2) -
		      (src_array_2 ? 1 : 0))
	{
		/* marshal array strings to buffer */
		size_t size = 1024;
		for (;;) {

			_buffer.construct(_alloc, size);

			unsigned dst_i = 0; /* index into destination array */

			auto try_append_entries = [&] (char const * const * const src_array,
			                               unsigned skip = 0)
			{
				if (!src_array)
					return;

				size_t const n = _num_entries(src_array);
				for (unsigned i = skip; i < n; i++) {
					array[dst_i] = _buffer->pos_ptr();
					if (!_buffer->try_append(src_array[i]))
						break;
					dst_i++;
				}
			};

			try_append_entries(src_array_1);
			try_append_entries(src_array_2, 1); /* skip old argv0 */

			bool const done = (dst_i == count);
			if (done) {
				array[dst_i] = nullptr;
				break;
			}
			size += 1024;
		}
	}

	~String_array() { _alloc.free(array, _array_bytes); }
};


/* pointer to environment, provided by libc */
extern char **environ;

static Env                             *_env_ptr;
static Allocator                       *_alloc_ptr;
static void                            *_user_stack_ptr;
static main_fn_ptr                      _main_ptr;
static Libc::String_array              *_env_vars_ptr;
static Libc::String_array              *_args_ptr;
static Libc::Reset_malloc_heap         *_reset_malloc_heap_ptr;
static Libc::Binary_name               *_binary_name_ptr;
static Libc::File_descriptor_allocator *_fd_alloc_ptr;


void Libc::init_execve(Env &env, Genode::Allocator &alloc, void *user_stack_ptr,
                       Reset_malloc_heap &reset_malloc_heap, Binary_name &binary_name,
                       File_descriptor_allocator &fd_alloc)
{
	_env_ptr               = &env;
	_alloc_ptr             = &alloc;
	_user_stack_ptr        =  user_stack_ptr;
	_reset_malloc_heap_ptr = &reset_malloc_heap;
	_binary_name_ptr       = &binary_name;
	_fd_alloc_ptr          = &fd_alloc;

	Dynamic_linker::keep(env, "libc.lib.so");
	Dynamic_linker::keep(env, "libm.lib.so");
	Dynamic_linker::keep(env, "posix.lib.so");
	Dynamic_linker::keep(env, "vfs.lib.so");
}


static void user_entry(void *)
{
	_env_ptr->exec_static_constructors();

	exit((*_main_ptr)(_args_ptr->count, _args_ptr->array, _env_vars_ptr->array));
}


extern "C" int execve(char const *, char *const[], char *const[]) __attribute__((weak));

extern "C" int execve(char const *filename,
                      char *const argv[], char *const envp[])
{
	if (!_env_ptr || !_alloc_ptr) {
		error("missing call of 'init_execve'");
		return Libc::Errno(EACCES);
	}

	/* close all file descriptors with the close-on-execve flag enabled */
	while (Libc::File_descriptor *fd = _fd_alloc_ptr->any_cloexec_libc_fd())
		close(fd->libc_fd);

	/* capture environment variables and args to libc-internal heap */
	Libc::String_array *saved_env_vars =
		new (*_alloc_ptr) Libc::String_array(*_alloc_ptr, envp);

	/*
	 * Resolve path of the executable and handle shebang arguments
	 */
	Libc::Absolute_path resolved_path;

	Libc::Interpreter::Path path(filename);

	Libc::String_array *saved_args =
		new (*_alloc_ptr) Libc::String_array(*_alloc_ptr, argv);

	enum { MAX_INTERPRETER_NESTING_LEVELS = 4 };

	for (unsigned i = 0; i < MAX_INTERPRETER_NESTING_LEVELS; i++) {

		try {
			Libc::resolve_symlinks(path.string(), resolved_path); }
		catch (Libc::Symlink_resolve_error) {
			warning("execve: executable binary '", filename, "' does not exist");
			return Libc::Errno(ENOENT);
		}

		Constructible<Libc::Interpreter> interpreter { };

		try {
			interpreter.construct(*_env_ptr, resolved_path.string()); }
		catch (...) {
			warning("execve: executable binary inaccessible as ROM module");
			return Libc::Errno(ENOENT);
		}

		if (interpreter->elf_executable())
			break;

		if (!interpreter->script()) {
			warning("invalid executable binary format: ", resolved_path.string());
			return Libc::Errno(ENOEXEC);
		}

		path = interpreter->path();

		/* concatenate shebang args with command-line args */
		Libc::String_array * const orig_saved_args = saved_args;

		saved_args = new (*_alloc_ptr)
			Libc::String_array(*_alloc_ptr, interpreter->args, argv);

		destroy(*_alloc_ptr, orig_saved_args);
	}

	try {
		_main_ptr = Dynamic_linker::respawn<main_fn_ptr>(*_env_ptr, resolved_path.string(), "main");
	}
	catch (Dynamic_linker::Invalid_symbol) {
		error("Dynamic_linker::respawn could not obtain binary entry point");
		return Libc::Errno(EACCES);
	}
	catch (Dynamic_linker::Invalid_rom_module) {
		error("Dynamic_linker::respawn could not access binary ROM");
		return Libc::Errno(EACCES);
	}

	/* purge line buffers, which may be allocated at the application heap */
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);

	/*
	 * Reconstruct malloc heap for application-owned data
	 */
	_reset_malloc_heap_ptr->reset_malloc_heap();

	Libc::Allocator app_heap { };

	_env_vars_ptr = new (app_heap) Libc::String_array(app_heap, saved_env_vars->array);
	_args_ptr     = new (app_heap) Libc::String_array(app_heap, saved_args->array);

	/* register list of environment variables at libc 'environ' pointer */
	environ = _env_vars_ptr->array;

	/* remember name of new ROM module, to be used by next call of fork */
	*_binary_name_ptr = Libc::Binary_name(resolved_path.string());

	destroy(_alloc_ptr, saved_env_vars);
	destroy(_alloc_ptr, saved_args);

	call_func(_user_stack_ptr, (void *)user_entry, nullptr);
}

extern "C" int _execve(char const *, char *const[], char *const[]) __attribute__((weak, alias("execve")));
