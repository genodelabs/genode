/*
 * \brief   Representation of a common instruction processor
 * \author  Martin Stein
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/multiprocessor.h>

using namespace Kernel;


Multiprocessor * Kernel::multiprocessor()
{
	static Multiprocessor s;
	return &s;
}
