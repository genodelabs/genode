/*
 * \brief   MMU initialization for Cortex A9 SMP
 * \author  Stefan Kalkowski
 * \date    2015-12-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spin_lock.h>
#include <util/mmio.h>

#include <platform.h>

/* entrypoint for non-boot CPUs */
extern "C" void * _start_setup_stack;


/**
 * SMP-safe simple counter
 */
class Cpu_counter
{
	private:

		Hw::Spin_lock _lock;
		volatile int _value = 0;

	public:

		void inc()
		{
			Hw::Spin_lock::Guard guard(_lock);
			Genode::memory_barrier();
			_value++;
		}

		void wait_for(int const v) {
			while (_value < v) ; }
};


struct Scu : Genode::Mmio
{
	struct Cr : Register<0x0, 32>
	{
		struct Enable : Bitfield<0, 1> { };
	};

	struct Dcr : Register<0x30, 32>
	{
		struct Bit_0 : Bitfield<0, 1> { };
	};

	struct Iassr : Register<0xc, 32>
	{
		struct Cpu0_way : Bitfield<0, 4> { };
		struct Cpu1_way : Bitfield<4, 4> { };
		struct Cpu2_way : Bitfield<8, 4> { };
		struct Cpu3_way : Bitfield<12, 4> { };
	};

	Scu() : Genode::Mmio(Board::Cpu_mmio::SCU_MMIO_BASE) { }

	void invalidate()
	{
		Iassr::access_t iassr = 0;
		for (Iassr::access_t way = 0; way <= Iassr::Cpu0_way::mask();
		     way++) {
			Iassr::Cpu0_way::set(iassr, way);
			Iassr::Cpu1_way::set(iassr, way);
			Iassr::Cpu2_way::set(iassr, way);
			Iassr::Cpu3_way::set(iassr, way);
			write<Iassr>(iassr);
		}
	}

	void enable(bool err_arm_764369)
	{
		if (err_arm_764369) write<Dcr::Bit_0>(1);
		write<Cr::Enable>(1);
	}
};


/*
 * The initialization of Cortex A9 multicore systems implies a sophisticated
 * algorithm in early revisions of this cpu.
 *
 * See ARM's Cortex-A9 MPCore TRM r2p0 in section 5.3.5 for more details
 */
unsigned Bootstrap::Platform::enable_mmu()
{
	using namespace Bootstrap;

	static volatile bool primary_cpu = true;
	static Cpu_counter   data_cache_invalidated;
	static Cpu_counter   data_cache_enabled;
	static Cpu_counter   smp_coherency_enabled;

	bool primary = primary_cpu;
	if (primary) primary_cpu = false;

	Cpu::Sctlr::init();
	Cpu::Cpsr::init();
	Actlr::disable_smp();

	/* locally initialize interrupt controller */
	pic.init_cpu_local();

	Cpu::invalidate_data_cache();
	data_cache_invalidated.inc();

	/* primary cpu wakes up all others */
	if (primary && NR_OF_CPUS > 1)
		Cpu::wake_up_all_cpus(&_start_setup_stack);

	/* wait for other cores' data cache invalidation */
	data_cache_invalidated.wait_for(NR_OF_CPUS);

	if (primary) {
		Scu scu;
		scu.invalidate();
		::Board::L2_cache l2_cache(::Board::PL310_MMIO_BASE);
		l2_cache.disable();
		l2_cache.invalidate();
		scu.enable(Cpu::errata(Cpu::ARM_764369));
	}

	/* secondary cpus wait for the primary's cache activation */
	if (!primary) data_cache_enabled.wait_for(1);

	Cpu::enable_mmu_and_caches((Genode::addr_t)core_pd->table_base);

	data_cache_enabled.inc();
	Cpu::clean_invalidate_data_cache();

	/* wait for other cores' data cache activation */
	data_cache_enabled.wait_for(NR_OF_CPUS);

	if (primary) {
		::Board::L2_cache l2_cache(board.core_mmio.virt_addr(::Board::PL310_MMIO_BASE));
		l2_cache.enable();
	}

	/* secondary cpus wait for the primary's coherency activation */
	if (!primary) smp_coherency_enabled.wait_for(1);

	Actlr::enable_smp();
	smp_coherency_enabled.inc();

	/*
	 * strangely, some older versions (imx6) seem to not work cache coherent
	 * until SMP bit is set, so write back the variable here.
	 */
	Cpu::clean_invalidate_data_cache();

	/* wait for other cores' coherency activation */
	smp_coherency_enabled.wait_for(NR_OF_CPUS);

	return Cpu::Mpidr::Aff_0::get(Cpu::Mpidr::read());
}
