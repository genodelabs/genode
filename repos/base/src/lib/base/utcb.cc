/*
 * \brief  Accessor to user-level thread-control block (UTCB)
 * \author Norman Feske
 * \date   2016-01-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>


Genode::Native_utcb *Genode::Thread::utcb() { return &_stack->utcb(); }
