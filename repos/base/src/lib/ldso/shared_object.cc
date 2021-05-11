/**
 * \brief  Implentation of Genode's shared-object interface
 * \author Sebastian Sumpf
 * \date   2015-03-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <linker.h>


/*************
 ** Helpers **
 *************/

static Linker::Root_object const &to_root(void *h)
{
	return *static_cast<Linker::Root_object const *>(h);
}


static Linker::Object *find_obj(Genode::addr_t addr)
{
	Linker::Object *elf = 0;
	Linker::for_each_object([&] (Linker::Object &e) {
		if (elf) return;

		if (addr >= e.link_map().addr  && addr < e.link_map().addr + e.size())
			elf = &e;
	});

	if (elf)
		return elf;

	throw Genode::Address_info::Invalid_address();
}


/*********
 ** API **
 *********/

Genode::Shared_object::Shared_object(Env &env, Allocator &md_alloc,
                                     char const *file, Bind bind, Keep keep)
:
	_md_alloc(md_alloc)
{
	using namespace Linker;

	if (verbose_shared)
		log("LD: open '", file ? file : "binary", "'");

	try {
		Mutex::Guard guard(Linker::shared_object_mutex());

		Root_object *root  = new (md_alloc)
			Root_object(env, md_alloc, file ? file : binary_name(),
			            bind == BIND_NOW ? Linker::BIND_NOW : Linker::BIND_LAZY,
			            keep == KEEP     ? Linker::KEEP     : Linker::DONT_KEEP);

		_handle = root;

		/* print loaded object information */
		if (Linker::verbose) {
			root->deps().for_each([] (Linker::Dependency const &dep) {
				if (dep.obj().already_present()) return;
				Linker::dump_link_map(dep.obj());
			});
		}

	} catch(Linker::Not_found &symbol) {
		warning("LD: symbol not found: '", symbol, "'");
		throw Invalid_file();

	} catch (...) {
		if (Linker::verbose)
			warning("LD: exception during Shared_object open: "
			        "'", Current_exception(), "'");

		throw Invalid_file();
	}
}


void *Genode::Shared_object::_lookup(const char *name) const
{
	using namespace Linker;

	if (verbose_shared)
		log("LD: shared object lookup '", name, "'");

	try {
		Mutex::Guard guard(Linker::mutex());

		Root_object const &root = to_root(_handle);

		Elf::Addr base;
		Elf::Sym const *symbol = lookup_symbol(name, *root.first_dep(), &base, true);

		return (void *)(base + symbol->st_value);
	} catch (...) { throw Shared_object::Invalid_symbol(); }
}


Genode::Shared_object::Link_map const &Genode::Shared_object::link_map() const
{
	return (Link_map const &)to_root(_handle).link_map();
}


Genode::Shared_object::~Shared_object()
{
	using namespace Linker;

	if (verbose_shared)
		log("LD: close shared object");

	Mutex::Guard guard(Linker::shared_object_mutex());
	destroy(_md_alloc, &const_cast<Root_object &>(to_root(_handle)));
}


Genode::Address_info::Address_info(addr_t address)
{
	using namespace Genode;

	if (verbose_shared)
		log("LD: address-info request: ", Hex(address));

	Linker::Object *e = find_obj(address);

	path = e->name();
	base = e->reloc_base();

	Linker::Object::Symbol_info const symbol = e->symbol_at_address(address);

	addr = symbol.addr;
	name = symbol.name;

	if (verbose_shared)
		log("LD: found address info: obj: ", path, " sym: ", name,
		    " addr: ", Hex(addr));
}


