/*
 * \brief Test Ada exceptions in C++
 * \author Johannes Kliemann
 * \date 2018-06-25
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/component.h>

extern "C" void except__raise_task();

void Component::construct(Genode::Env &env)
{
	Genode::log("Ada exception test");

	except__raise_task();

	env.parent().exit(0);
}
