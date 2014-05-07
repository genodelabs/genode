/*
 * \brief  Implementation of Thread API interface for core
 * \author Norman Feske
 * \date   2006-05-03
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>

using namespace Genode;

void Thread_base::_init_platform_thread() { }
void Thread_base::_deinit_platform_thread() { }
void Thread_base::start() { }
void Thread_base::cancel_blocking() { }
