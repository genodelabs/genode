/*
 * \brief   Platform thread interface implementation - x86 specific
 * \author  Alexander Boettcher
 * \date    2017-08-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform_thread.h>
#include <arch_kernel_object.h>

using Genode::Phys_allocator;
using Genode::Allocator;

Phys_allocator& Genode::phys_alloc_16k(Allocator * core_mem_alloc)
{
	static Genode::Phys_allocator phys_alloc_16k(core_mem_alloc);
	return phys_alloc_16k;
}

void Genode::Platform_thread::affinity(Affinity::Location location)
{
	_location = location;
	seL4_TCB_SetAffinity(tcb_sel().value(), location.xpos());
}

bool Genode::Thread_info::init_vcpu(Platform &platform, Cap_sel ept)
{
	enum { PAGES_16K = (1UL << Vcpu_kobj::SIZE_LOG2) / 4096 };

	this->vcpu_state_phys = Untyped_memory::alloc_pages(phys_alloc_16k(), PAGES_16K);
	this->vcpu_sel        = platform.core_sel_alloc().alloc();

	seL4_Untyped const service = Untyped_memory::_core_local_sel(Core_cspace::TOP_CNODE_UNTYPED_16K, vcpu_state_phys, Vcpu_kobj::SIZE_LOG2).value();

	create<Vcpu_kobj>(service, platform.core_cnode().sel(), vcpu_sel);
	seL4_Error res = seL4_X86_VCPU_SetTCB(vcpu_sel.value(), tcb_sel.value());
	if (res != seL4_NoError)
		return false;

	int error = seL4_TCB_SetEPTRoot(tcb_sel.value(), ept.value());
	if (error != seL4_NoError)
		return false;

	return true;
}
