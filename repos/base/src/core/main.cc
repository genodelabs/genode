/*
 * \brief  Core main program
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* core includes */
#include <platform.h>
#include <core_region_map.h>
#include <core_service.h>
#include <signal_transmitter.h>
#include <system_control.h>
#include <rom_root.h>
#include <rm_root.h>
#include <cpu_root.h>
#include <pd_root.h>
#include <log_root.h>
#include <io_mem_root.h>
#include <irq_root.h>
#include <trace/root.h>
#include <platform_services.h>
#include <core_child.h>
#include <pager.h>

Core::Platform &Core::platform_specific()
{
	static Platform _platform;
	return _platform;
}


Core::Platform_generic &Core::platform() { return platform_specific(); }


Core::Trace::Source_registry &Core::Trace::sources()
{
	static Source_registry inst;
	return inst;
}


namespace Genode { extern char const *version_string; }


struct Genode::Platform { };


/*
 * Executed on the initial stack
 */
Genode::Platform &Genode::init_platform()
{
	init_stack_area();

	static Platform platform { };
	return platform;
}


/*
 * Executed on a stack located within the stack area
 */
void Genode::bootstrap_component(Genode::Platform &)
{
	using namespace Core;

	Range_allocator &ram_ranges      = Core::platform().ram_alloc();
	Rom_fs          &rom_modules     = Core::platform().rom_fs();
	Range_allocator &io_mem_ranges   = Core::platform().io_mem_alloc();
	Range_allocator &io_port_ranges  = Core::platform().io_port_alloc();
	Range_allocator &irq_ranges      = Core::platform().irq_alloc();
	Allocator       &core_alloc      = platform_specific().core_mem_alloc();

	Ram_quota const avail_ram  { ram_ranges.avail() };
	Cap_quota const avail_caps { Core::platform().max_caps() };

	static constexpr size_t STACK_SIZE = 20 * 1024;

	static Rpc_entrypoint ep { nullptr, STACK_SIZE, "entrypoint", Affinity::Location() };

	static Core::Core_account core_account { ep, avail_ram, avail_caps };

	static Ram_dataspace_factory core_ram {
		ep, ram_ranges, Ram_dataspace_factory::any_phys_range(), core_alloc };

	static Core_region_map core_rm { ep };

	static Rpc_entrypoint &signal_ep = core_signal_ep(ep);

	init_exception_handling(core_ram, core_rm);
	init_core_signal_transmitter(signal_ep);
	init_page_fault_handling(ep);

	/* disable tracing within core because it is not fully implemented */
	inhibit_tracing = true;

	log("Genode ", Genode::version_string);

	static Core::Trace::Policy_registry trace_policies;

	static Registry<Service> services;

	/*
	 * Allocate session meta data on distinct dataspaces to enable independent
	 * destruction (to enable quota trading) of session component objects.
	 */
	static Sliced_heap sliced_heap { core_ram, core_rm };

	/*
	 * Factory for creating RPC capabilities within core
	 */
	static Rpc_cap_factory rpc_cap_factory { sliced_heap };

	static Pager_entrypoint pager_ep(rpc_cap_factory);

	using Trace_root              = Core::Trace::Root;
	using Trace_session_component = Core::Trace::Session_component;

	static Core::System_control &system_control = init_system_control(sliced_heap, ep);

	static Rom_root    rom_root    (ep, ep, rom_modules, sliced_heap);
	static Rm_root     rm_root     (ep, sliced_heap, core_ram, core_rm, pager_ep);
	static Cpu_root    cpu_root    (core_ram, core_rm, ep, ep, pager_ep,
	                                sliced_heap, Core::Trace::sources());
	static Pd_root     pd_root     (ep, signal_ep, pager_ep, ram_ranges, core_rm, sliced_heap,
	                                platform_specific().core_mem_alloc(),
	                                system_control);
	static Log_root    log_root    (ep, sliced_heap);
	static Io_mem_root io_mem_root (ep, ep, io_mem_ranges, ram_ranges, sliced_heap);
	static Irq_root    irq_root    (irq_ranges, sliced_heap);
	static Trace_root  trace_root  (core_ram, core_rm, ep, sliced_heap,
	                                Core::Trace::sources(), trace_policies);

	static Core_service<Rom_session_component>    rom_service    (services, rom_root);
	static Core_service<Rm_session_component>     rm_service     (services, rm_root);
	static Core_service<Cpu_session_component>    cpu_service    (services, cpu_root);
	static Core_service<Pd_session_component>     pd_service     (services, pd_root);
	static Core_service<Log_session_component>    log_service    (services, log_root);
	static Core_service<Io_mem_session_component> io_mem_service (services, io_mem_root);
	static Core_service<Irq_session_component>    irq_service    (services, irq_root);
	static Core_service<Trace_session_component>  trace_service  (services, trace_root);

	/* make platform-specific services known to service pool */
	platform_add_local_services(ep, sliced_heap, services, Core::Trace::sources(),
	                            core_ram, core_rm, io_port_ranges);

	if (!core_account.ram_account.try_withdraw({ 224*1024 })) {
		error("core preservation exceeds available RAM");
		return;
	}

	if (!core_account.cap_account.try_withdraw({ 1000 })) {
		error("core preservation exceeds available caps");
		return;
	}

	Ram_quota const init_ram_quota = core_account.ram_account.avail();
	Cap_quota const init_cap_quota = core_account.cap_account.avail();

	/* CPU session representing core */
	static Cpu_session_component
		core_cpu(ep,
		         Session::Resources{{Cpu_session::RAM_QUOTA},
		                            {Cpu_session::CAP_QUOTA}},
		         "core", Session::Diag{false},
		         core_ram, core_rm, ep, pager_ep, Core::Trace::sources(), "",
		         Affinity::unrestricted(), Cpu_session::QUOTA_LIMIT);

	log(init_ram_quota.value / (1024*1024), " MiB RAM and ",
	    init_cap_quota, " caps assigned to init");

	static Reconstructible<Core::Core_child>
		init(services, ep, core_rm, core_ram, core_account,
		     core_cpu, core_cpu.cap(), init_cap_quota, init_ram_quota);

	Core::platform().wait_for_exit();

	init.destruct();
}
