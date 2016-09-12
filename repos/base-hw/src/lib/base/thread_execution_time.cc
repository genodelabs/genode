/*
 * \brief  Implementation of Thread::execution_time()
 * \author Johannes Schlatow
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/internal/native_utcb.h>

using namespace Genode;

unsigned long long Thread::execution_time()
{
	return this->utcb()->execution_time();
}
