/*
 * \brief  Test for cross library linking
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2011-07-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#include <base/printf.h>
#include "test-ldso.h"

using namespace Genode;

void lib_2_exception() { throw 668; }

Lib_2_global::Lib_2_global()      { printf("%s %x\n", __func__, --x); }
void Lib_2_global::lib_2_global() { printf("%s %x\n", __func__, --x); }
Lib_2_global::~Lib_2_global()     { printf("%s %x\n", __func__, --x); x=0; }

Lib_2_global lib_2_global;

Lib_2_local::Lib_2_local()      { printf("%s %x\n", __func__, --x); }
void Lib_2_local::lib_2_local() { printf("%s %x\n", __func__, --x); }
Lib_2_local::~Lib_2_local()     { printf("%s %x\n", __func__, --x); x=0; }

Lib_2_local * lib_2_local()
{
	static Lib_2_local s;
	return &s;
}

unsigned lib_2_pod_1 { 0x87654321 };
