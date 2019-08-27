/**
 * \brief  Manage object dependencies
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <linker.h>
#include <dynamic.h>
#include <init.h>


/**
 * Dependency node
 */
Linker::Dependency::Dependency(Env &env, Allocator &md_alloc,
                               char const *path, Root_object *root,
                               Fifo<Dependency> &deps,
                               Keep keep)
:
	_obj(load(env, md_alloc, path, *this, keep)),
	_root(root),
	_md_alloc(&md_alloc)
{
	deps.enqueue(*this);
	load_needed(env, *_md_alloc, deps, keep);
}


Linker::Dependency::~Dependency()
{
	if (!_unload_on_destruct)
		return;

	if (!_obj.unload())
		return;

	if (verbose_loading)
		log("Destroy: ", _obj.name());

	destroy(_md_alloc, &_obj);
}


bool Linker::Dependency::in_dep(char const *file, Fifo<Dependency> const &dep)
{
	bool result = false;

	dep.for_each([&] (Dependency const &d) {
		if (!result && !strcmp(file, d.obj().name()))
			result = true;
	});

	return result;
}


void Linker::Dependency::_load(Env &env, Allocator &alloc, char const *path,
                               Fifo<Dependency> &deps, Keep keep)
{
	if (!in_dep(Linker::file(path), deps))
		new (alloc) Dependency(env, alloc, path, _root, deps, keep);

	/* re-order initializer list, if needed object has been already added */
	else if (Object *o = Init::list()->contains(Linker::file(path)))
		Init::list()->reorder(o);
}


void Linker::Dependency::preload(Env &env, Allocator &alloc,
                                 Fifo<Dependency> &deps, Config const &config)
{
	config.for_each_library([&] (Config::Rom_name const &rom, Keep keep) {
		_load(env, alloc, rom.string(), deps, keep); });
}


void Linker::Dependency::load_needed(Env &env, Allocator &md_alloc,
                                     Fifo<Dependency> &deps, Keep keep)
{
	_obj.dynamic().for_each_dependency([&] (char const *path) {
		_load(env, md_alloc, path, deps, keep); });
}


Linker::Dependency const &Linker::Dependency::first() const
{
	return _root ? *_root->first_dep() : *this;
}


Linker::Root_object::Root_object(Env &env, Allocator &md_alloc,
                                 char const *path, Bind bind, Keep keep)
:
	_md_alloc(md_alloc)
{
	/*
	 * The life time of 'Dependency' objects is managed via reference
	 * counting. Hence, we don't need to remember them here.
	 */
	new (md_alloc) Dependency(env, md_alloc, path, this, _deps, keep);

	/* provide Genode base library access */
	new (md_alloc) Dependency(env, md_alloc, linker_name(), this, _deps, DONT_KEEP);

	/* relocate and call constructors */
	try {
		Init::list()->initialize(bind, Linker::stage);
	} catch (...) {
		Init::list()->flush();
		_deps.dequeue_all([&] (Dependency &d) {
			destroy(_md_alloc, &d); });
		throw;
	}
}
