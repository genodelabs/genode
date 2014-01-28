/*
 * \brief  Thread related C helpers
 * \author Martin Stein
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>


/**
 * Return top end of the stack of the calling thread
 */
extern "C" void * my_stack_top()
{
	return Genode::Thread_base::myself()->stack_top();
}
