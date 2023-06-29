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
#include <base/service.h>
#include <base/child.h>
#include <base/component.h>
#include <rm_session/connection.h>
#include <pd_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* core includes */
#include <platform.h>
#include <core_env.h>
#include <core_service.h>
#include <signal_transmitter.h>
#include <rom_root.h>
#include <rm_root.h>
#include <cpu_root.h>
#include <pd_root.h>
#include <log_root.h>
#include <io_mem_root.h>
#include <irq_root.h>
#include <trace/root.h>
#include <platform_services.h>

using namespace Core;


/***************************************
 ** Core environment/platform support **
 ***************************************/

Core_env &Core::core_env()
{
	/*
	 * Make sure to initialize the platform before constructing the core
	 * environment.
	 */
	platform();

	/*
	 * By placing the environment as static object here, we ensure that its
	 * constructor gets called when this function is used the first time.
	 */
	static Core_env _env;

	/*
	 * Register signal-source entrypoint at core-local signal-transmitter back
	 * end
	 */
	static bool signal_transmitter_initialized;

	if (!signal_transmitter_initialized)
		signal_transmitter_initialized =
			(init_core_signal_transmitter(_env.signal_ep()), true);

	return _env;
}


Core::Platform &Core::platform_specific()
{
	static Platform _platform;
	return _platform;
}


Platform_generic &Core::platform() { return platform_specific(); }


struct Genode::Platform { };


Genode::Platform &Genode::init_platform()
{
	core_env();
	static Genode::Platform platform { };
	return platform;
}


/**
 * Dummy implementation for core that has no parent to ask for resources
 */
void Genode::init_parent_resource_requests(Genode::Env &) {};


/****************
 ** Core child **
 ****************/

class Core_child : public Child_policy
{
	private:

		Registry<Service> &_services;

		Capability<Pd_session>  _core_pd_cap;
		Pd_session             &_core_pd;

		Capability<Cpu_session> _core_cpu_cap;
		Cpu_session            &_core_cpu;

		Cap_quota const _cap_quota;
		Ram_quota const _ram_quota;

		Child _child;

	public:

		/**
		 * Constructor
		 */
		Core_child(Registry<Service> &services, Region_map &local_rm,
		           Pd_session  &core_pd,  Capability<Pd_session>  core_pd_cap,
		           Cpu_session &core_cpu, Capability<Cpu_session> core_cpu_cap,
		           Cap_quota cap_quota, Ram_quota ram_quota,
		           Rpc_entrypoint &ep)
		:
			_services(services),
			_core_pd_cap (core_pd_cap),  _core_pd (core_pd),
			_core_cpu_cap(core_cpu_cap), _core_cpu(core_cpu),
			_cap_quota(Child::effective_quota(cap_quota)),
			_ram_quota(Child::effective_quota(ram_quota)),
			_child(local_rm, ep, *this)
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "init"; }

		Route resolve_session_request(Service::Name const &name,
		                              Session_label const &label,
		                              Session::Diag const  diag) override
		{
			Service *service = nullptr;
			_services.for_each([&] (Service &s) {
				if (!service && s.name() == name)
					service = &s; });

			if (!service)
				throw Service_denied();

			return Route { .service = *service,
			               .label   = label,
			               .diag    = diag };
		}

		void init(Pd_session &session, Capability<Pd_session> cap) override
		{
			session.ref_account(_core_pd_cap);
			_core_pd.transfer_quota(cap, _cap_quota);
			_core_pd.transfer_quota(cap, _ram_quota);
		}

		void init(Cpu_session &session, Capability<Cpu_session> cap) override
		{
			session.ref_account(_core_cpu_cap);
			_core_cpu.transfer_quota(cap, Cpu_session::quota_lim_upscale(100, 100));
		}

		Pd_session           &ref_pd()           override { return _core_pd; }
		Pd_session_capability ref_pd_cap() const override { return _core_pd_cap; }

		size_t session_alloc_batch_size() const override { return 128; }
};


/****************
 ** Signal API **
 ****************/

/*
 * In contrast to non-core components, core disables the signal thread by
 * overriding 'Genode::init_signal_thread' with a dummy. Within core, the
 * signal thread is not needed as core is never supposed to receive any
 * signals.
 */

void Genode::init_signal_thread(Env &) { }
void Genode::destroy_signal_thread()   { }


/*******************
 ** Trace support **
 *******************/

Core::Trace::Source_registry &Core::Trace::sources()
{
	static Source_registry inst;
	return inst;
}


/***************
 ** Core main **
 ***************/

namespace Genode {
	extern bool        inhibit_tracing;
	extern char const *version_string;
}


void Genode::bootstrap_component(Genode::Platform &)
{
	init_exception_handling(*core_env().pd_session(), core_env().local_rm());

	/* disable tracing within core because it is not fully implemented */
	inhibit_tracing = true;

	log("Genode ", Genode::version_string);

	static Core::Trace::Policy_registry trace_policies;

	static Rpc_entrypoint &ep              =  core_env().entrypoint();
	static Ram_allocator  &core_ram_alloc  =  core_env().ram_allocator();
	static Region_map     &local_rm        =  core_env().local_rm();
	Pd_session            &core_pd         = *core_env().pd_session();
	Capability<Pd_session> core_pd_cap     =  core_env().pd_session_cap();

	static Registry<Service> services;

	/*
	 * Allocate session meta data on distinct dataspaces to enable independent
	 * destruction (to enable quota trading) of session component objects.
	 */
	static Sliced_heap sliced_heap(core_ram_alloc, local_rm);

	/*
	 * Factory for creating RPC capabilities within core
	 */
	static Rpc_cap_factory rpc_cap_factory(sliced_heap);

	static Pager_entrypoint pager_ep(rpc_cap_factory);

	using Trace_root              = Core::Trace::Root;
	using Trace_session_component = Core::Trace::Session_component;

	static Rom_root    rom_root    (ep, ep, platform().rom_fs(), sliced_heap);
	static Rm_root     rm_root     (ep, sliced_heap, core_ram_alloc, local_rm, pager_ep);
	static Cpu_root    cpu_root    (core_ram_alloc, local_rm, ep, ep, pager_ep,
	                                sliced_heap, Core::Trace::sources());
	static Pd_root     pd_root     (ep, core_env().signal_ep(), pager_ep,
	                                platform().ram_alloc(),
	                                local_rm, sliced_heap,
	                                platform_specific().core_mem_alloc());
	static Log_root    log_root    (ep, sliced_heap);
	static Io_mem_root io_mem_root (ep, ep, platform().io_mem_alloc(),
	                                platform().ram_alloc(), sliced_heap);
	static Irq_root    irq_root    (*core_env().pd_session(),
	                                platform().irq_alloc(), sliced_heap);
	static Trace_root  trace_root  (core_ram_alloc, local_rm, ep, sliced_heap,
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
	platform_add_local_services(ep, sliced_heap, services, Core::Trace::sources());

	size_t const avail_ram_quota = core_pd.avail_ram().value;
	size_t const avail_cap_quota = core_pd.avail_caps().value;

	size_t const preserved_ram_quota = 224*1024;
	size_t const preserved_cap_quota = 1000;

	if (avail_ram_quota < preserved_ram_quota) {
		error("core preservation exceeds platform RAM limit");
		return;
	}

	if (avail_cap_quota < preserved_cap_quota) {
		error("core preservation exceeds platform cap quota limit");
		return;
	}

	Ram_quota const init_ram_quota { avail_ram_quota - preserved_ram_quota };
	Cap_quota const init_cap_quota { avail_cap_quota - preserved_cap_quota };

	/* CPU session representing core */
	static Cpu_session_component
		core_cpu(ep,
		         Session::Resources{{Cpu_session::RAM_QUOTA},
		                            {Cpu_session::CAP_QUOTA}},
		         "core", Session::Diag{false},
		         core_ram_alloc, local_rm, ep, pager_ep, Core::Trace::sources(), "",
		         Affinity::unrestricted(), Cpu_session::QUOTA_LIMIT);

	Cpu_session_capability core_cpu_cap = core_cpu.cap();

	log("", init_ram_quota.value / (1024*1024), " MiB RAM and ", init_cap_quota, " caps "
	    "assigned to init");

	static Reconstructible<Core_child>
		init(services, local_rm, core_pd, core_pd_cap, core_cpu, core_cpu_cap,
		     init_cap_quota, init_ram_quota, ep);

	platform().wait_for_exit();

	init.destruct();
}
