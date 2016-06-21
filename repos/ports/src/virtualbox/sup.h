/*
 * \brief  Common VirtualBox SUPLib supplements
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_H_
#define _SUP_H_

/* Genode includes */
#include <cpu_session/cpu_session.h>

/* VirtualBox includes */
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/com/ptr.h>
#include <iprt/param.h>

#include "MachineImpl.h"

/**
 * Returns true if a vCPU could be started. If false we run without
 * hardware acceleration support.
 */
bool create_emt_vcpu(pthread_t * pthread, size_t stack,
                     const pthread_attr_t *attr,
                     void *(*start_routine)(void *), void *arg,
                     Genode::Cpu_session * cpu_session,
                     Genode::Affinity::Location location,
                     unsigned int cpu_id);


uint64_t genode_cpu_hz();
void genode_update_tsc(void (*update_func)(void), unsigned long update_us);

Genode::Cpu_session * get_vcpu_cpu_session();

void genode_VMMR0_DO_GVMM_CREATE_VM(PSUPVMMR0REQHDR pReqHdr);
void genode_VMMR0_DO_GVMM_REGISTER_VMCPU(PVMR0 pVMR0, VMCPUID idCpu);

HRESULT genode_setup_machine(ComObjPtr<Machine> machine);

HRESULT genode_check_memory_config(ComObjPtr<Machine> machine);

#endif /* _SUP_H_ */
