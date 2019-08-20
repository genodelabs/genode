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

/* libc includes */
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/* libc-internal includes */
#include <libc/allocator.h>
#include <internal/call_func.h>
#include <libc_init.h>
#include <libc_errno.h>
#include <task.h>

using namespace Genode;

typedef int (*main_fn_ptr) (int, char **, char **);

namespace Libc { struct String_array; }


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

	String_array(Allocator &alloc, char const * const * const src_array)
	:
		_alloc(alloc), count(_num_entries(src_array))
	{
		/* marshal array strings to buffer */
		size_t size = 1024;
		for (;;) {

			_buffer.construct(alloc, size);

			unsigned i = 0;
			for (i = 0; i < count; i++) {
				array[i] = _buffer->pos_ptr();
				if (!_buffer->try_append(src_array[i]))
					break;
			}

			bool const done = (i == count);
			if (done) {
				array[i] = nullptr;
				break;
			}

			warning("env buffer ", size, " too small");
			size += 1024;
		}
	}

	~String_array() { _alloc.free(array, _array_bytes); }
};


/* pointer to environment, provided by libc */
extern char **environ;

static Env                     *_env_ptr;
static Allocator               *_alloc_ptr;
static void                    *_user_stack_ptr;
static main_fn_ptr              _main_ptr;
static Libc::String_array      *_env_vars_ptr;
static Libc::String_array      *_args_ptr;
static Libc::Reset_malloc_heap *_reset_malloc_heap_ptr;


void Libc::init_execve(Env &env, Genode::Allocator &alloc, void *user_stack_ptr,
                       Reset_malloc_heap &reset_malloc_heap)
{
	_env_ptr               = &env;
	_alloc_ptr             = &alloc;
	_user_stack_ptr        =  user_stack_ptr;
	_reset_malloc_heap_ptr = &reset_malloc_heap;

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

	/* capture environment variables and args to libc-internal heap */
	Libc::String_array *saved_env_vars =
		new (*_alloc_ptr) Libc::String_array(*_alloc_ptr, envp);

	Libc::String_array *saved_args =
		new (*_alloc_ptr) Libc::String_array(*_alloc_ptr, argv);

	try {
		_main_ptr = Dynamic_linker::respawn<main_fn_ptr>(*_env_ptr, filename, "main");
	}
	catch (Dynamic_linker::Invalid_symbol) {
		error("Dynamic_linker::respawn could not obtain binary entry point");
		return Libc::Errno(EACCES);
	}
	catch (Dynamic_linker::Invalid_rom_module) {
		error("Dynamic_linker::respawn could not access binary ROM");
		return Libc::Errno(EACCES);
	}

	/*
	 * Reconstruct malloc heap for application-owned data
	 */
	_reset_malloc_heap_ptr->reset_malloc_heap();

	Libc::Allocator app_heap { };

	_env_vars_ptr = new (app_heap) Libc::String_array(app_heap, saved_env_vars->array);
	_args_ptr     = new (app_heap) Libc::String_array(app_heap, saved_args->array);

	/* register list of environment variables at libc 'environ' pointer */
	environ = _env_vars_ptr->array;

	destroy(_alloc_ptr, saved_env_vars);
	destroy(_alloc_ptr, saved_args);

	call_func(_user_stack_ptr, (void *)user_entry, nullptr);
}

extern "C" int _execve(char const *, char *const[], char *const[]) __attribute__((weak, alias("execve")));
