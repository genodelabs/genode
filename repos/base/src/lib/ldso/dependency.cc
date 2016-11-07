/**
 * \brief  Manage object dependencies
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <linker.h>
#include <dynamic.h>
#include <init.h>


/**
 * Dependency node
 */
Linker::Dependency::Dependency(char const *path, Root_object *root,
                               Genode::Fifo<Dependency> * const dep,
                               unsigned flags)
	: obj(load(path, this, flags)), root(root)
{
	dep->enqueue(this);
	load_needed(dep, flags);
}


Linker::Dependency::~Dependency()
{
	if (obj->unload()) {

		if (verbose_loading)
			Genode::log("Destroy: ", obj->name());

		destroy(Genode::env()->heap(), obj);
	}
}


bool Linker::Dependency::in_dep(char const *file,
                                Genode::Fifo<Dependency> * const dep)
{
	for (Dependency *d = dep->head(); d; d = d->next())
		if (!Genode::strcmp(file, d->obj->name()))
			return true;

	return false;
}


void Linker::Dependency::load_needed(Genode::Fifo<Dependency> * const dep,
                                     unsigned flags)
{
	for (Dynamic::Needed *n = obj->dynamic()->needed.head(); n; n = n->next()) {
		char const *path = n->path(obj->dynamic()->strtab);

		Object *o;
		if (!in_dep(Linker::file(path), dep))
			new (Genode::env()->heap()) Dependency(path, root, dep, flags);

		/* re-order initializer list, if needed object has been already added */
		else if ((o = Init::list()->contains(Linker::file(path))))
			Init::list()->reorder(o);
	}
}


Linker::Root_object::Root_object(char const *path, unsigned flags)
{
	new (Genode::env()->heap()) Dependency(path, this, &dep, flags);

	/* provide Genode base library access */
	new (Genode::env()->heap()) Dependency(linker_name(), this, &dep);

	/* relocate and call constructors */
	Init::list()->initialize();
}
