/*
 * \brief  Suplib global info page implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_GIP_H_
#define _SUP_GIP_H_

/* VirtualBox includes */
#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/param.h>
#include <iprt/uint128.h>
#include <VBox/sup.h>

/* Genode includes */
#include <base/entrypoint.h>
#include <base/signal.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>

namespace Sup {
	using namespace Genode;

	struct Gip;
}

class Sup::Gip
{
	private:

		struct Entrypoint : Genode::Entrypoint
		{
			SUPGIPCPU *cpu;

			Genode::uint64_t const cpu_hz;

			Timer::Connection          timer;
			Signal_handler<Entrypoint> handler;

			Entrypoint(Env &env, SUPGIPCPU *cpu, Genode::uint64_t cpu_hz)
			:
				Genode::Entrypoint(env, 512*1024, "gip_ep", Affinity::Location()),
				cpu(cpu), cpu_hz(cpu_hz),
				timer(env), handler(*this, *this, &Entrypoint::update)
			{
				timer.sigh(handler);
				timer.trigger_periodic(UPDATE_US);
			}

			void update()
			{
				Genode::uint64_t tsc_current = Genode::Trace::timestamp();

				/*
				 * Convert tsc to nanoseconds.
				 *
				 * There is no 'uint128_t' type on x86_32, so we use the 128-bit type
				 * and functions provided by VirtualBox.
				 *
				 * nanots128 = tsc_current * 1000*1000*1000 / genode_cpu_hz()
				 *
				 */

				RTUINT128U nanots128;
				RTUInt128AssignU64(&nanots128, tsc_current);

				RTUINT128U multiplier;
				RTUInt128AssignU32(&multiplier, 1'000'000'000);
				RTUInt128AssignMul(&nanots128, &multiplier);

				RTUINT128U divisor;
				RTUInt128AssignU64(&divisor, cpu_hz);
				RTUInt128AssignDiv(&nanots128, &divisor);

				/*
				 * Transaction id must be incremented before and after update,
				 * read struct SUPGIPCPU description for more details.
				 */
				ASMAtomicIncU32(&cpu->u32TransactionId);

				cpu->u64TSC    = tsc_current;
				cpu->u64NanoTS = nanots128.s.Lo;

				/*
				 * Transaction id must be incremented before and after update,
				 * read struct SUPGIPCPU description for more details.
				 */
				ASMAtomicIncU32(&cpu->u32TransactionId);
			}
		};

		enum {
			UPDATE_HZ  = 10'000,
			UPDATE_US  = 1'000'000/UPDATE_HZ,
			UPDATE_NS  = 1'000*UPDATE_US
		};

		Genode::size_t const  _gip_size;
		SUPGLOBALINFOPAGE    &_gip;

	public:

		Gip(Env &env, Cpu_count cpu_count, Cpu_freq_khz cpu_khz)
		:
			_gip_size(RT_ALIGN_Z(RT_UOFFSETOF_DYN(SUPGLOBALINFOPAGE, aCPUs[cpu_count.value]),
			                     PAGE_SIZE)),
			_gip(*(SUPGLOBALINFOPAGE *)RTMemPageAllocZ(_gip_size))
		{
			Genode::uint64_t const cpu_hz = 1'000ull*cpu_khz.value;

			/* checked by TMR3Init */
			_gip.u32Magic              = SUPGLOBALINFOPAGE_MAGIC;
			_gip.u32Version            = SUPGLOBALINFOPAGE_VERSION;
			_gip.u32Mode               = SUPGIPMODE_SYNC_TSC;
			_gip.cCpus                 = cpu_count.value;
			_gip.cPages                = _gip_size/PAGE_SIZE;
			_gip.u32UpdateHz           = UPDATE_HZ;
			_gip.u32UpdateIntervalNS   = UPDATE_NS;
			_gip.u64NanoTSLastUpdateHz = 0;
			_gip.u64CpuHz              = cpu_hz;
			_gip.cOnlineCpus           = cpu_count.value;
			_gip.cPresentCpus          = cpu_count.value;
			_gip.cPossibleCpus         = cpu_count.value;
			_gip.cPossibleCpuGroups    = 1;
			_gip.idCpuMax              = cpu_count.value - 1;
			_gip.enmUseTscDelta        = SUPGIPUSETSCDELTA_NOT_APPLICABLE;
			/* evaluated by rtTimeNanoTSInternalRediscover in Runtime/common/time/timesup.cpp */
			_gip.fGetGipCpu            = SUPGIPGETCPU_APIC_ID;

			/* from SUPDrvGip.cpp */
			for (unsigned i = 0; i < RT_ELEMENTS(_gip.aiCpuFromCpuSetIdx); ++i)
				_gip.aiCpuFromCpuSetIdx[i] = UINT16_MAX;

			SUPGIPCPU *cpu = _gip.aCPUs;

			/* XXX in SUPGIPMODE_SYNC_TSC only the first CPU's TSC is updated */
			Entrypoint &ep = *new Entrypoint(env, cpu, cpu_hz);

			for (unsigned i = 0; i < cpu_count.value; ++i) {
				cpu[i].u32TransactionId        = 0;
				cpu[i].u32UpdateIntervalTSC    = cpu_hz/UPDATE_HZ;
				cpu[i].u64NanoTS               = 0ull;
				cpu[i].u64TSC                  = 0ull;
				cpu[i].u64CpuHz                = cpu_hz;
				cpu[i].cErrors                 = 0;
				cpu[i].iTSCHistoryHead         = 0;
				cpu[i].u32PrevUpdateIntervalNS = UPDATE_NS;
				cpu[i].enmState                = SUPGIPCPUSTATE_ONLINE;
				cpu[i].idCpu                   = i;
				cpu[i].iCpuSet                 = 0;
				cpu[i].idApic                  = i;
			}
		}

		SUPGLOBALINFOPAGE *gip() { return &_gip; };
};

#endif /* _SUP_GIP_H_ */
