/*
 * \brief  Kernel backend for Vcpus when having no virtualization
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/thread.h>

void Kernel::Thread::_call_new_vcpu()    { user_arg_0(-1); }
void Kernel::Thread::_call_delete_vcpu() { user_arg_0(-1); }
void Kernel::Thread::_call_run_vcpu()    { user_arg_0(-1); }
void Kernel::Thread::_call_pause_vcpu()  { user_arg_0(-1); }
