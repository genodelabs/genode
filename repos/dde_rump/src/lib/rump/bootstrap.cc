/*
 * \brief  Call initialization functions for all modules and components
 * \author Sebastian Sumpf
 * \date   2013-12-12
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {
#include <sys/types.h>
#include <rump/rumpuser.h>
#include <exec_elf.h>
}

#include <rump/env.h>

#include <base/log.h>
#include <base/shared_object.h>
#include <util/string.h>


#ifdef _LP64
typedef Elf64_Dyn Elf_Dyn;
typedef Elf64_Sym Elf_Sym;
#else
typedef Elf32_Dyn Elf_Dyn;
typedef Elf32_Sym Elf_Sym;
#endif

static bool const verbose = false;


static Genode::Shared_object *obj_main;


static Genode::Env &env()
{
	return Rump::env().env();
}


static Genode::Allocator &heap()
{
	return Rump::env().heap();
}


void rumpuser_dl_bootstrap(rump_modinit_fn domodinit, rump_symload_fn symload,
                           rump_compload_fn compload)
{
	/* open main program and request link map */
	using namespace Genode;

	try {
		obj_main = new (heap()) Shared_object(::env(), heap(), nullptr,
		                                      Shared_object::BIND_NOW,
		                                      Shared_object::KEEP);
	}
	catch (...) { error("could not dlopen the main executable"); return; }
}


void * rumpuser_dl_globalsym(const char *symname)
{

	void *addr = 0;
	try { addr = obj_main->lookup(symname); }
	catch (...) { }

	if (verbose)
		Genode::log("Lookup: ", symname, " addr ", addr);

	return addr;
}
