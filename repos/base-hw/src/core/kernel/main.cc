/*
 * \brief  Main object of the kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2021-07-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw Core includes */
#include <kernel/cpu.h>
#include <kernel/lock.h>
#include <kernel/main.h>

/* base-hw-internal includes */
#include <hw/boot_info.h>


namespace Kernel {

	class Main;
}


class Kernel::Main
{
	private:

		friend void main_handle_kernel_entry();
		friend void main_initialize_and_handle_kernel_entry();

		static Main *_instance;

		void _handle_kernel_entry();
};


Kernel::Main *Kernel::Main::_instance;


void Kernel::Main::_handle_kernel_entry()
{
	Cpu &cpu = cpu_pool().cpu(Cpu::executing_id());
	Cpu_job * new_job;

	{
		Lock::Guard guard(data_lock());

		new_job = &cpu.schedule();
	}

	new_job->proceed(cpu);
}


void Kernel::main_handle_kernel_entry()
{
	Main::_instance->_handle_kernel_entry();
}


void Kernel::main_initialize_and_handle_kernel_entry()
{
	static volatile bool lock_ready   = false;
	static volatile bool pool_ready   = false;
	static volatile bool kernel_ready = false;

	/**
	 * It is essential to guard the initialization of the data_lock object
	 * in the SMP case, because otherwise the __cxa_guard_aquire of the cxx
	 * library contention path might get called, which ends up in
	 * calling a Semaphore, which will call Kernel::stop_thread() or
	 * Kernel::yield() system-calls in this code
	 */
	while (Cpu::executing_id() != Cpu::primary_id() && !lock_ready) { }

	/**
	 * Create a main object and initialize static reference to it
	 */
	if (Cpu::executing_id() == Cpu::primary_id()) {

		static Main instance { };
		Main::_instance = &instance;
	}

	{
		Lock::Guard guard(data_lock());

		lock_ready = true;

		/* initialize current cpu */
		pool_ready = cpu_pool().initialize();
	};

	/* wait until all cpus have initialized their corresponding cpu object */
	while (!pool_ready) { ; }

	/* the boot-cpu initializes the rest of the kernel */
	if (Cpu::executing_id() == Cpu::primary_id()) {
		Lock::Guard guard(data_lock());

		using Boot_info = Hw::Boot_info<Board::Boot_info>;
		Boot_info &boot_info {
			*reinterpret_cast<Boot_info*>(Hw::Mm::boot_info().base) };

		cpu_pool().for_each_cpu([&] (Kernel::Cpu &cpu) {
			boot_info.kernel_irqs.add(cpu.timer().interrupt_id());
		});
		boot_info.kernel_irqs.add((unsigned)Board::Pic::IPI);

		Genode::log("");
		Genode::log("kernel initialized");

		Core_main_thread::singleton();
		kernel_ready = true;
	} else {
		/* secondary cpus spin until the kernel is initialized */
		while (!kernel_ready) {;}
	}

	Main::_instance->_handle_kernel_entry();
}


Kernel::time_t Kernel::main_read_idle_thread_execution_time(unsigned cpu_idx)
{
	return cpu_pool().cpu(cpu_idx).idle_thread().execution_time();
}
