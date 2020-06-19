/*
 * \brief  Testing CPU performance
 * \author Stefan Kalkowski
 * \date   2018-10-22
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>

#include <bogomips.h>

void Component::construct(Genode::Env &)
{
	using namespace Genode;

	log("Cpu testsuite started");

	size_t cnt = 1000*1000*1000 / bogomips_instr_count() * 10;

	log("Execute 10G BogoMIPS in ", cnt, " rounds with ",
	    bogomips_instr_count(), " instructions each");
	bogomips(cnt);
	log("Finished execution");
};
