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

using namespace Kernel;

capid_t Thread::_call_vcpu_create(Core::Kernel_object<Vcpu> &, Call_arg,
                                  Board::Vcpu_state &, Vcpu::Identity &,
                                  capid_t) {
	return cap_id_invalid(); }

void Thread::_call_vcpu_destroy(Core::Kernel_object<Vcpu>&) { }
void Thread::_call_vcpu_run(capid_t const) { }
void Thread::_call_vcpu_pause(capid_t const) { }
