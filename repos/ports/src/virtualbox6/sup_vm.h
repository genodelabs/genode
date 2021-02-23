/*
 * \brief  Suplib VM implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_VM_H_
#define _SUP_VM_H_

/* VirtualBox includes */
#include <iprt/assert.h>
#include <VBox/vmm/gvm.h>

/* local includes */
#include <sup.h>

namespace Sup {
	struct Vm;
	struct Vcpu_handler;
}

struct Sup::Vm : GVM
{
	void init(PSUPDRVSESSION psession, Cpu_count cpu_count);

	static Vm & create(PSUPDRVSESSION psession, Cpu_count cpu_count);

	class Cpu_index_out_of_range : Exception { };

	void register_vcpu_handler(Cpu_index cpu_index, Vcpu_handler &handler);

	template <typename FN>
	void with_vcpu_handler(Cpu_index cpu_index, FN const &fn)
	{
		if (cpu_index.value >= VM::cCpus)
			throw Cpu_index_out_of_range();

		VMCPU &cpu = GVM::aCpus[cpu_index.value];

		Vcpu_handler * const handler_ptr = (Vcpu_handler *)cpu.pVCpuR0ForVtg;

		Assert(handler_ptr);

		fn(*handler_ptr);
	}
};

#endif /* _SUP_VM_H_ */

