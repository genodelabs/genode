/**
 * \brief  Initialization list (calls ctors)
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
struct Linker::Init : Genode::List<Object>
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
			if (!Genode::strcmp(file, elf->name()))
				return elf;

		return nullptr;
	}

	void reorder(Object *elf)
	{
		/* put in front of initializer list */
		remove(elf);
		insert(elf);

		/* re-order dependencies */
		for (Dynamic::Needed *n = elf->dynamic()->needed.head(); n; n = n->next()) {
			char const *path = n->path(elf->dynamic()->strtab);
			Object *e;

			if ((e = contains(Linker::file(path))))
				reorder(e);
		}
	}

	void initialize()
	{
		Object *obj = first();

		/* relocate */
		for (; obj; obj = obj->next_init()) {
			if (verbose_relocation)
				Genode::log("Relocate ", obj->name());
			obj->relocate();
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

			if (obj->dynamic()->init_function) {

				if (verbose_relocation)
					Genode::log(obj->name(), " init func ", obj->dynamic()->init_function);

				obj->dynamic()->init_function();
			}

			obj = restart ? first() : next;
			restart = false;
		}

		in_progress = false;
	}
};

#endif /* _INCLUDE__INIT_H_ */
