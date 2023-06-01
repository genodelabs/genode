/*
 * \brief  Dynamic linker interface bindings
 * \author Sebastian Sumpf
 * \date   2014-10-24
 *
 * Wrap Genode's shared library interface onto libc semantics.
 */

/* Genode includes */
#include <base/log.h>
#include <base/shared_object.h>

/* Genode-specific libc includes */
#include <libc/allocator.h>

/* libc-internal includes */
#include <internal/init.h>

/* libc includes */
extern "C" {
#include <dlfcn.h>
}

using namespace Genode;


using Err_string = String<128>;


static Err_string &_err_str()
{
	static Err_string str { };
	return str;
}


/**
 * Set global 'dlerror' message
 */
template <typename... ARGS>
static void _dlerror(ARGS &&... args) { _err_str() = { args... }; }


char *dlerror(void)
{
	return (char *)_err_str().string();
}


static Genode::Env *genode_env = nullptr;


namespace Libc {

	void init_dl(Genode::Env &env)
	{
		if (!genode_env)
			genode_env = &env;
	}
}


static Shared_object *to_object(void *handle)
{
	return static_cast<Shared_object *>(handle);
}


void *dlopen(const char *name, int mode)
{
	if (mode & RTLD_GLOBAL)
		warning("ignoring unsupported RTLD_GLOBAL in dlopen()");

	int supported = RTLD_LAZY | RTLD_NOW | RTLD_LOCAL | RTLD_GLOBAL | RTLD_NODELETE;

	/* error on unsupported mode values */
	if (mode & ~supported) {
		_dlerror("Unsupported mode ", Hex(mode & ~supported));
		error(__func__, ": ", _err_str());
		return nullptr;
	}

	Shared_object::Bind const bind =
		(mode & RTLD_NOW) ? Shared_object::BIND_NOW : Shared_object::BIND_LAZY;

	Shared_object::Keep const keep =
		(mode & RTLD_NODELETE) ? Shared_object::KEEP : Shared_object::DONT_KEEP;

	if (!genode_env) {
		error(__func__, ": support for dynamic linking not initialized");
		return nullptr;
	}

	try {
		static Libc::Allocator global_alloc;

		return new (global_alloc)
			Shared_object(*genode_env, global_alloc,
			              name ? Genode::Path<128>(name).last_element() : nullptr, /* extract file name */
			              bind, keep);
	} catch (...) {
		_dlerror("Unable to open file ", name);
	}
	return nullptr;
}


void *dlsym(void *handle, const char *name)
{
	if (handle == nullptr || handle == RTLD_NEXT || handle == RTLD_SELF) {
		_dlerror("Unsupported handle ", handle);
		return nullptr;
	}

	try {
		if (handle == RTLD_DEFAULT) {
			static Libc::Allocator global_alloc;

			return Shared_object(*genode_env, global_alloc, nullptr,
			                     Shared_object::BIND_LAZY,
			                     Shared_object::KEEP).lookup(name);
		}

		return to_object(handle)->lookup(name);
	} catch (...) {
		_dlerror("Symbol '", name, "' not found");
	}

	return nullptr;
}


int dladdr(const void *addr, Dl_info *dlip)
{
	try {
		Address_info info((addr_t)addr);
		dlip->dli_fname = info.path;
		dlip->dli_fbase = (void *)info.base;
		dlip->dli_sname = info.name;
		dlip->dli_saddr = (void *)info.addr;
	} catch (...) {
		_dlerror("Symbol at ", addr, " not found");
		return 0;
	}

	return 1;
}


int dlclose(void *handle)
{
	destroy(Libc::Allocator(), to_object(handle));
	return 0;
}


int dlinfo(void *handle, int request, void *p)
{
	error(__func__, " not implemented");
	return 0;
}


dlfunc_t dlfunc(void *handle, const char *name)
{
	error(__func__, " not implemented");
	return 0;
}


void *dlvsym(void *handle, const char *name, const char *version)
{
	error(__func__, " not implemented");
	return nullptr;
}
