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

/* core includes */
#include <map_local.h>
#include <kernel/cpu.h>
#include <kernel/lock.h>
#include <kernel/main.h>
#include <platform_pd.h>
#include <platform_thread.h>

/* base-hw-internal includes */
#include <hw/boot_info.h>

namespace Kernel { class Main; }


class Kernel::Main
{
	private:

		friend void main_handle_kernel_entry();
		friend void main_initialize_and_handle_kernel_entry();
		friend time_t main_read_idle_thread_execution_time(unsigned cpu_idx);
		friend void main_print_char(char c);

		enum { SERIAL_BAUD_RATE = 115200 };

		static Main *_instance;

		Lock                                    _data_lock           { };
		Cpu_pool                                _cpu_pool            { };
		Irq::Pool                               _user_irq_pool       { };
		Board::Address_space_id_allocator       _addr_space_id_alloc { };
		Core::Core_platform_pd                  _core_platform_pd    { _addr_space_id_alloc };
		Genode::Constructible<Core_main_thread> _core_main_thread    { };
		Board::Global_interrupt_controller      _global_irq_ctrl     { };
		Board::Serial                           _serial              { Core::Platform::mmio_to_virt(Board::UART_BASE),
		                                                               Board::UART_CLOCK,
		                                                               SERIAL_BAUD_RATE };

		void _handle_kernel_entry();

	public:

		static Core::Platform_pd &core_platform_pd();
};


Kernel::Main *Kernel::Main::_instance;


void Kernel::Main::_handle_kernel_entry()
{
	Cpu &cpu = _cpu_pool.cpu(Cpu::executing_id());
	Cpu_job * new_job;

	{
		Lock::Guard guard(_data_lock);

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
	using Boot_info = Hw::Boot_info<Board::Boot_info>;

	static Lock              init_lock;
	static volatile unsigned nr_of_initialized_cpus { 0 };
	static volatile bool     kernel_initialized     { false };

	Boot_info &boot_info {
		*reinterpret_cast<Boot_info*>(Hw::Mm::boot_info().base) };

	unsigned const nr_of_cpus  { boot_info.cpus };

	/**
	 * Let the first CPU create a Main object and initialize the static
	 * reference to it.
	 */
	{
		Lock::Guard guard(init_lock);

		static Main instance;
		Main::_instance = &instance;

	}

	/* the CPU resumed if the kernel is already initialized */
	if (kernel_initialized) {

		{
			Lock::Guard guard(Main::_instance->_data_lock);

			if (nr_of_initialized_cpus == nr_of_cpus) {
				nr_of_initialized_cpus = 0;

				Main::_instance->_serial.init();
				Main::_instance->_global_irq_ctrl.init();
			}

			nr_of_initialized_cpus = nr_of_initialized_cpus + 1;

			Main::_instance->_cpu_pool.cpu(Cpu::executing_id()).reinit_cpu();

			if (nr_of_initialized_cpus == nr_of_cpus) {
				Genode::raw("kernel resumed");
			}
		}

		while (nr_of_initialized_cpus < nr_of_cpus) { }

		Main::_instance->_handle_kernel_entry();
		/* never reached */
		return;
	}

	{
		/**
		 * Let each CPU initialize its corresponding CPU object in the
		 * CPU pool.
		 */
		Lock::Guard guard(Main::_instance->_data_lock);
		Main::_instance->_cpu_pool.initialize_executing_cpu(
			Main::_instance->_addr_space_id_alloc,
			Main::_instance->_user_irq_pool,
			Main::_instance->_core_platform_pd.kernel_pd(),
			Main::_instance->_global_irq_ctrl);

		nr_of_initialized_cpus = nr_of_initialized_cpus + 1;
	};

	/**
	 * Let all CPUs block until each CPU object in the CPU pool has been
	 * initialized by the corresponding CPU.
	 */
	while (nr_of_initialized_cpus < nr_of_cpus) { }

	/**
	 * Let the primary CPU initialize the core main thread and finish
	 * initialization of the boot info.
	 */
	{
		Lock::Guard guard(Main::_instance->_data_lock);

		if (Cpu::executing_id() == Main::_instance->_cpu_pool.primary_cpu().id()) {
			Main::_instance->_cpu_pool.for_each_cpu([&] (Kernel::Cpu &cpu) {
				boot_info.kernel_irqs.add(cpu.timer().interrupt_id());
			});
			boot_info.kernel_irqs.add((unsigned)Board::Pic::IPI);

			Main::_instance->_core_main_thread.construct(
				Main::_instance->_addr_space_id_alloc,
				Main::_instance->_user_irq_pool,
				Main::_instance->_cpu_pool,
				Main::_instance->_core_platform_pd.kernel_pd());

			boot_info.core_main_thread_utcb =
				(addr_t)Main::_instance->_core_main_thread->utcb();

			Genode::log("");
			Genode::log("kernel initialized");
			kernel_initialized = true;
		}
	}

	/**
	 * Let secondary CPUs block until the primary CPU has initialized the
	 * core main thread and finished initialization of the boot info.
	 */
	while (!kernel_initialized) {;}

	Main::_instance->_handle_kernel_entry();
}


Core::Platform_pd &Kernel::Main::core_platform_pd()
{
	return _instance->_core_platform_pd;
}


void Kernel::main_print_char(char c)
{
	Main::_instance->_serial.put_char(c);
}


Kernel::time_t Kernel::main_read_idle_thread_execution_time(unsigned cpu_idx)
{
	return Main::_instance->_cpu_pool.cpu(cpu_idx).idle_thread().execution_time();
}


Core::Platform_pd &Core::Platform_thread::_kernel_main_get_core_platform_pd()
{
	return Kernel::Main::core_platform_pd();
}


bool Core::map_local(addr_t from_phys, addr_t to_virt, size_t num_pages,
                     Page_flags flags)
{
	return
		Kernel::Main::core_platform_pd().insert_translation(
			to_virt, from_phys, num_pages * get_page_size(), flags);
}


bool Core::unmap_local(addr_t virt_addr, size_t num_pages)
{
	Kernel::Main::core_platform_pd().flush(
		virt_addr, num_pages * get_page_size());

	return true;
}
