/*
 * \brief  Sanitizer test
 * \author Christian Prochaska
 * \date   2018-12-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>

extern void sanitizer_init(Genode::Env &);

void Component::construct(Genode::Env &env)
{
	sanitizer_init(env);

	/* test array out-of-bounds access detection */

	int array[1] = { 0 };
	int volatile i = 2;
	Genode::log("array[", i, "] = ", array[i]);

	/* test null pointer access detection */

	int * volatile ptr = nullptr;
	*ptr = 0x55;
}
