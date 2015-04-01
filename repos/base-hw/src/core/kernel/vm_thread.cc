/*
 * \brief  Kernel backend for VMs when having no virtualization
 * \author Martin Stein
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/thread.h>

void Kernel::Thread::_call_new_vm()    { user_arg_0(-1); }
void Kernel::Thread::_call_delete_vm() { user_arg_0(-1); }
void Kernel::Thread::_call_run_vm()    { user_arg_0(-1); }
void Kernel::Thread::_call_pause_vm()  { user_arg_0(-1); }
