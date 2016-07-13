/**
 * \brief  Implentation of Genode's shared-object interface
 * \author Sebastian Sumpf
 * \date   2015-03-11
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <linker.h>


/*************
 ** Helpers **
 *************/

static Linker::Root_object *to_root(void *h)
{
	return static_cast<Linker::Root_object *>(h);
}


/**
 * Needed during shared object creation and destruction, since global lists are
 * manipulated
 */
static Genode::Lock & shared_object_lock()
{
	static Genode::Lock _lock;
	return _lock;
}


static Linker::Object *find_obj(Genode::addr_t addr)
{
	for (Linker::Object *e = Linker::obj_list_head(); e; e = e->next_obj())
		if (addr >= e->link_map()->addr  && addr < e->link_map()->addr + e->size())
			return e;

	throw Genode::Address_info::Invalid_address();
}


/*********
 ** API **
 *********/

Genode::Shared_object::Shared_object(char const *file, unsigned flags)
{
	using namespace Linker;

	if (verbose_shared)
		Genode::log("LD: open '", file ? file : "binary", "'");

	try {
		Genode::Lock::Guard guard(shared_object_lock());

		/* update bind now variable */
		bind_now = (flags & Shared_object::NOW) ? true : false;

		_handle = (Root_object *)new (Genode::env()->heap()) Root_object(file ? file : binary_name(), flags);

		/* print loaded object information */
		try {
			if (Linker::verbose)
				Linker::dump_link_map(to_root(_handle)->dep.head()->obj);
		} catch (...) {  }

	} catch (...) { throw Invalid_file(); }
}


void *Genode::Shared_object::_lookup(const char *name) const
{
	using namespace Linker;

	if (verbose_shared)
		Genode::log("LD: shared object lookup '", name, "'");

	try {
		Genode::Lock::Guard guard(Object::lock());

		Elf::Addr       base;
		Root_object    *root   = to_root(_handle);
		Elf::Sym const *symbol = lookup_symbol(name, root->dep.head(), &base, true);

		return (void *)(base + symbol->st_value);
	} catch (...) { throw Shared_object::Invalid_symbol(); }
}


Genode::Shared_object::Link_map const * Genode::Shared_object::link_map() const
{
	return (Link_map const *)to_root(_handle)->link_map();
}


Genode::Shared_object::~Shared_object()
{
	using namespace Linker;

	if (verbose_shared)
		Genode::log("LD: close shared object");

	Genode::Lock::Guard guard(shared_object_lock());
	destroy(Genode::env()->heap(), to_root(_handle));
}


Genode::Address_info::Address_info(Genode::addr_t address)
{
	using namespace Genode;

	if (verbose_shared)
		Genode::log("LD: address-info request: ", Genode::Hex(address));

	Linker::Object *e = find_obj(address);
	e->info(address, *this);

	if (verbose_shared)
		Genode::log("LD: found address info: obj: ", path, " sym: ", name,
		            " addr: ", Genode::Hex(addr));
}


