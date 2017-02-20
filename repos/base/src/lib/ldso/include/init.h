/**
 * \brief  Initialization list (calls ctors)
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INIT_H_
#define _INCLUDE__INIT_H_

#include <linker.h>


namespace Linker {
	struct Init;
}


/**
 * Handle static construction and relocation of ELF files
 */
struct Linker::Init : List<Object>
{
	bool in_progress = false;
	bool restart     = false;

	static Init *list()
	{
		static Init _list;
		return &_list;
	}

	Object *contains(char const *file)
	{
		for (Object *elf = first(); elf; elf = elf->next_init())
			if (!strcmp(file, elf->name()))
				return elf;

		return nullptr;
	}

	void reorder(Object *elf)
	{
		/* put in front of initializer list */
		remove(elf);
		insert(elf);

		/* re-order dependencies */
		elf->dynamic().for_each_dependency([&] (char const *path) {

//		for (Dynamic::Needed *n = elf->dynamic().needed.head(); n; n = n->next()) {
//			char const *path = n->path(elf->dynamic().strtab);

			if (Object *e = contains(Linker::file(path)))
				reorder(e);
		});
	}

	void initialize(Bind bind)
	{
		Object *obj = first();

		/* relocate */
		for (; obj; obj = obj->next_init()) {
			if (verbose_relocation)
				log("Relocate ", obj->name());
			obj->relocate(bind);
		}

		/*
		 * Recursive initialization call is not allowed here. This might happend
		 * when Shared_objects (e.g. dlopen and friends) are constructed from within
		 * global constructors (ctors).
		 */
		if (in_progress) {
			restart = true;
			return;
		}

		in_progress = true;

		/* call static constructors */
		obj = first();
		while (obj) {

			Object *next = obj->next_init();
			remove(obj);

			obj->dynamic().call_init_function();

			obj = restart ? first() : next;
			restart = false;
		}

		in_progress = false;
	}
};

#endif /* _INCLUDE__INIT_H_ */
