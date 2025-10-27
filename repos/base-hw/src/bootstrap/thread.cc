/*
 * \brief  Thread implementation needed by cxx library
 * \author Stefan Kalkowski
 * \date   2016-10-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/thread.h>
#include <assert.h>


Genode::Thread * Genode::Thread::myself()
{
	ASSERT(false);
	return nullptr;
}


Genode::Thread::Stack_info Genode::Thread::mystack()
{
	ASSERT(false);
	return { };
}
