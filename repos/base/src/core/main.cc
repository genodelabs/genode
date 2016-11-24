/*
 * \brief  Core main program
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>
#include <base/sleep.h>
#include <base/service.h>
#include <base/child.h>
#include <base/log.h>
#include <rm_session/connection.h>
#include <pd_session/connection.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>

/* base-internal includes */
#include <base/internal/globals.h>

/* core includes */
#include <platform.h>
#include <core_env.h>
#include <ram_root.h>
#include <rom_root.h>
#include <rm_root.h>
#include <cpu_root.h>
#include <pd_root.h>
#include <log_root.h>
#include <io_mem_root.h>
#include <irq_root.h>
#include <trace/root.h>
#include <platform_services.h>

using namespace Genode;


/***************************************
 ** Core environment/platform support **
 ***************************************/

Core_env * Genode::core_env()
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
	return &_env;
}


Env_deprecated * Genode::env() {
	return core_env(); }


Platform *Genode::platform_specific()
{
	static Platform _platform;
	return &_platform;
}


Platform_generic *Genode::platform() { return platform_specific(); }


/*************************
 ** Core parent support **
 *************************/

Session_capability Core_parent::session(Parent::Client::Id          id,
                                        Parent::Service_name const &name,
                                        Parent::Session_args const &args,
                                        Affinity             const &affinity)
{
	Session_capability cap;

	_services.for_each([&] (Service &service) {

		if ((service.name() != name.string()) || cap.valid())
			return;

		Session_state &session = *new (_alloc)
			Session_state(service, _id_space, id, args.string(), affinity);

		service.initiate_request(session);

		cap = session.cap;
	});

	if (!cap.valid())
		error("unexpected core-parent ", name.string(), " session request");

	return cap;
}


/****************
 ** Core child **
 ****************/

class Core_child : public Child_policy
{
	private:

		/*
		 * Entry point used for serving the parent interface
		 */
		Rpc_entrypoint _entrypoint;
		enum { STACK_SIZE = 2 * 1024 * sizeof(Genode::addr_t)};

		Registry<Service> &_services;

		Capability<Ram_session> _core_ram_cap;
		Ram_session            &_core_ram;

		Capability<Cpu_session> _core_cpu_cap;
		Cpu_session            &_core_cpu;

		size_t const _ram_quota;

		Child _child;

	public:

		/**
		 * Constructor
		 */
		Core_child(Registry<Service> &services, Ram_session &core_ram,
		           Capability<Ram_session> core_ram_cap, size_t ram_quota,
		           Cpu_session &core_cpu, Capability<Cpu_session> core_cpu_cap)
		:
			_entrypoint(nullptr, STACK_SIZE, "init_child", false),
			_services(services),
			_core_ram_cap(core_ram_cap), _core_ram(core_ram),
			_core_cpu_cap(core_cpu_cap), _core_cpu(core_cpu),
			_ram_quota(Child::effective_ram_quota(ram_quota)),
			_child(*env()->rm_session(), _entrypoint, *this)
		{
			_entrypoint.activate();
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const { return "init"; }

		Service &resolve_session_request(Service::Name const &name,
		                                 Session_state::Args const &args) override
		{
			Service *service = nullptr;
			_services.for_each([&] (Service &s) {
				if (!service && s.name() == name)
					service = &s; });

			if (!service)
				throw Parent::Service_denied();

			return *service;
		}

		void init(Ram_session &session, Capability<Ram_session> cap) override
		{
			session.ref_account(_core_ram_cap);
			_core_ram.transfer_quota(cap, _ram_quota);
		}

		void init(Cpu_session &session, Capability<Cpu_session> cap) override
		{
			session.ref_account(_core_cpu_cap);
			_core_cpu.transfer_quota(cap, Cpu_session::quota_lim_upscale(100, 100));
		}

		Ram_session           &ref_ram()           { return _core_ram; }
		Ram_session_capability ref_ram_cap() const { return _core_ram_cap; }
};


/****************
 ** Signal API **
 ****************/

/*
 * In contrast to the 'Platform_env' used by non-core components, core disables
 * the signal thread but overriding 'Genode::init_signal_thread' with a dummy.
 * Within core, the signal thread is not needed as core is never supposed to
 * receive any signals. Otherwise, the signal thread would be the only
 * non-entrypoint thread within core, which would be a problem on NOVA where
 * the creation of regular threads within core is unsupported.
 */

namespace Genode { void init_signal_thread(Env &) { } }


/*******************
 ** Trace support **
 *******************/

Trace::Source_registry &Trace::sources()
{
	static Trace::Source_registry inst;
	return inst;
}


/***************
 ** Core main **
 ***************/

namespace Genode {
	extern bool        inhibit_tracing;
	extern char const *version_string;
}


int main()
{
	/**
	 * Disable tracing within core because it is currently not fully implemented.
	 */
	inhibit_tracing = true;

	log("Genode ", Genode::version_string);

	static Trace::Policy_registry trace_policies;

	/*
	 * Initialize root interfaces for our services
	 */
	Rpc_entrypoint *e = core_env()->entrypoint();

	Registry<Service> &services = core_env()->services();

	/*
	 * Allocate session meta data on distinct dataspaces to enable independent
	 * destruction (to enable quota trading) of session component objects.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/*
	 * Factory for creating RPC capabilities within core
	 */
	static Rpc_cap_factory rpc_cap_factory(sliced_heap);

	static Pager_entrypoint pager_ep(rpc_cap_factory);

	static Ram_root     ram_root     (e, e, platform()->ram_alloc(), &sliced_heap);
	static Rom_root     rom_root     (e, e, platform()->rom_fs(), &sliced_heap);
	static Rm_root      rm_root      (e, &sliced_heap, pager_ep);
	static Cpu_root     cpu_root     (e, e, &pager_ep, &sliced_heap,
	                                  Trace::sources());
	static Pd_root      pd_root      (e, e, pager_ep, &sliced_heap);
	static Log_root     log_root     (e, &sliced_heap);
	static Io_mem_root  io_mem_root  (e, e, platform()->io_mem_alloc(),
	                                  platform()->ram_alloc(), &sliced_heap);
	static Irq_root     irq_root     (core_env()->pd_session(),
	                                  platform()->irq_alloc(), &sliced_heap);
	static Trace::Root  trace_root   (e, &sliced_heap, Trace::sources(), trace_policies);

	static Core_service<Rom_session_component>    rom_service    (services, rom_root);
	static Core_service<Ram_session_component>    ram_service    (services, ram_root);
	static Core_service<Rm_session_component>     rm_service     (services, rm_root);
	static Core_service<Cpu_session_component>    cpu_service    (services, cpu_root);
	static Core_service<Pd_session_component>     pd_service     (services, pd_root);
	static Core_service<Log_session_component>    log_service    (services, log_root);
	static Core_service<Io_mem_session_component> io_mem_service (services, io_mem_root);
	static Core_service<Irq_session_component>    irq_service    (services, irq_root);
	static Core_service<Trace::Session_component> trace_service  (services, trace_root);

	/* make platform-specific services known to service pool */
	platform_add_local_services(e, &sliced_heap, &services);

	/* create CPU session representing core */
	static Cpu_session_component
		core_cpu(e, e, &pager_ep, &sliced_heap, Trace::sources(),
		         "label=\"core\"", Affinity(), Cpu_session::QUOTA_LIMIT);
	Cpu_session_capability core_cpu_cap = core_env()->entrypoint()->manage(&core_cpu);

	/*
	 * Transfer all left memory to init, but leave some memory left for core
	 *
	 * NOTE: exception objects thrown in core components are currently
	 * allocated on core's heap and not accounted by the component's meta data
	 * allocator
	 */

	Genode::size_t const ram_quota = platform()->ram_alloc()->avail() - 224*1024;
	log("", ram_quota / (1024*1024), " MiB RAM assigned to init");

	static Volatile_object<Core_child>
		init(services, *env()->ram_session(), env()->ram_session_cap(),
		     ram_quota, core_cpu, core_cpu_cap);

	platform()->wait_for_exit();

	init.destruct();
	return 0;
}
