/*
 * \brief  Guest-interface manager support
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-12-03
 *
 * The GIM KVM device is a mechanism for providing a stable time source
 * to the guest. The hypervisor provides a pair of TSC value, nanosecond
 * value along with a conversion factor (TSC <-> nanosecond) to the guest.
 *
 * - The values are communicated on memory shared between guest and VMM
 *
 * - The location of the guest-physical address is picked by the guest
 *   and propagated to the hypervisor via the MSR MSR_KVM_SYSTEM_TIME_NEW
 *   (0x4b564d01).
 *
 * - The values on the shared page are supposed to be periodically updated.
 *   Apparently, VirtualBox updates the values only when the MSR is written.
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/misc_math.h>

/* VirtualBox includes */
#include <GIMInternal.h>      /* needed for access to VM::gim.s */
#include <GIMKvmInternal.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/vmcc.h>

/* local includes */
#include <sup.h>

using namespace Genode;


/*
 * This function must be called by the vCPU handler when detecting an MSR-write
 * VM exit for MSR_KVM_SYSTEM_TIME_NEW before entering the VirtualBox code
 * (which calls gimKvmWriteMsr). Since we are never executing any R0 code, the
 * pKvmCpu value would remain undefined when arriving the the following
 * assertion:
 *
 *   Assert(pKvmCpu->uTsc);
 *   Assert(pKvmCpu->uVirtNanoTS);
 *
 * The implementation roughly corresponds to 'gimR0KvmUpdateSystemTime'
 */
void Sup::update_gim_system_time(VM &vm, VMCPU &vmcpu)
{
	using ::uint64_t;

	uint64_t uTsc        = 0;
	uint64_t uVirtNanoTS = 0;

	enum { MAX_MEASUREMENT_DURATION_NS = 400U };

	/* calculate max duration from tsc */
	PSUPGLOBALINFOPAGE pGip      = g_pSUPGlobalInfoPage;
	uint64_t           max_ticks = MAX_MEASUREMENT_DURATION_NS;

	if(pGip && pGip->u64CpuHz) {
		/* round */
		uint64_t ticks_per_ns = (pGip->u64CpuHz + 500'000'000) / 1'000'000'000;
		max_ticks = max(ticks_per_ns * MAX_MEASUREMENT_DURATION_NS, max_ticks);
	}

	/*
	 * If we got preempted during the measurement, repeat.
	 */
	for (unsigned round = 1; ; ++round) {

		uTsc                      = TMCpuTickGetNoCheck(&vmcpu) | UINT64_C(1);
		uVirtNanoTS               = TMVirtualGetNoCheck(&vm)    | UINT64_C(1);
		uint64_t const uTsc_again = TMCpuTickGetNoCheck(&vmcpu) | UINT64_C(1);


		if (uTsc_again - uTsc < max_ticks)
			break;

		if (round > 3 && round % 2 == 0)
			warning("preemption during measurement, uTsc=", uTsc,
			        " uTsc_again=", uTsc_again, " uVirtNanoTS=", uVirtNanoTS,
			        " max_ticks=", max_ticks);
	}

	for (VMCPUID idCpu = 0; idCpu < vm.cCpus; idCpu++) {

		PGIMKVMCPU pKvmCpu = &VMCC_GET_CPU(&vm, idCpu)->gim.s.u.KvmCpu;

		if (!pKvmCpu->uTsc && !pKvmCpu->uVirtNanoTS) {
			pKvmCpu->uTsc        = uTsc;
			pKvmCpu->uVirtNanoTS = uVirtNanoTS;
		}
	}
}

