/*
 * \brief  Test for the CPU sampler component
 * \author Christian Prochaska
 * \date   2016-01-18
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>

void __attribute((noinline)) func()
{
	for (;;) {
		/* define an exact label to support -fno-omit-frame-poiner */
		asm volatile (".global label_in_loop\nlabel_in_loop:");
	}
}

extern int label_in_loop;

void Component::construct(Genode::Env &)
{
	Genode::log("Test started. func: ", &label_in_loop);

	func();
}
