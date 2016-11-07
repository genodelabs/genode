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


/* pool of provided core services */
static Service_registry local_services;


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

Session_capability Core_parent::session(Parent::Service_name const &name,
                                        Parent::Session_args const &args,
                                        Affinity             const &affinity)
{
	Service *service = local_services.find(name.string());

	if (service)
		return service->session(args.string(), affinity);

	warning("service_name=\"", name.string(), "\" args=\"", args.string(), "\" not handled");
	return Session_capability();
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

		Service_registry &_local_services;

		/*
		 * Dynamic linker, does not need to be valid because init is statically
		 * linked
		 */
		Dataspace_capability _ldso_ds;

		Pd_session_client  _pd;
		Ram_session_client _ram;
		Cpu_session_client _cpu;

		Child::Initial_thread _initial_thread { _cpu, _pd, "init_main" };

		Region_map_client _address_space;

		Child _child;

	public:

		/**
		 * Constructor
		 */
		Core_child(Dataspace_capability elf_ds, Pd_session_capability pd,
		           Ram_session_capability ram,
		           Cpu_session_capability cpu,
		           Service_registry &services)
		:
			_entrypoint(nullptr, STACK_SIZE, "init_child", false),
			_local_services(services),
			_pd(pd), _ram(ram), _cpu(cpu),
			_address_space(Pd_session_client(pd).address_space()),
			_child(elf_ds, _ldso_ds, _pd, _pd, _ram, _ram, _cpu, _initial_thread,
			       *env()->rm_session(), _address_space, _entrypoint, *this,
			       *_local_services.find(Pd_session::service_name()),
			       *_local_services.find(Ram_session::service_name()),
			       *_local_services.find(Cpu_session::service_name()))
		{
			_entrypoint.activate();
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		void filter_session_args(const char *, char *args,
					 Genode::size_t args_len)
		{
			using namespace Genode;

			char label_buf[Parent::Session_args::MAX_SIZE];
			Arg_string::find_arg(args, "label").string(label_buf, sizeof(label_buf), "");

			char value_buf[Parent::Session_args::MAX_SIZE];
			Genode::snprintf(value_buf, sizeof(value_buf),
			                 "\"%s%s%s\"",
			                 "init",
			                 Genode::strcmp(label_buf, "") == 0 ? "" : " -> ",
			                 label_buf);

			Arg_string::set_arg(args, args_len, "label", value_buf);
		}


		const char *name() const { return "init"; }

		Service *resolve_session_request(const char *service, const char *)
		{
			return _local_services.find(service);
		}
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

	/*
	 * Play our role as parent of init and declare our services.
	 */

	static Local_service ls[] = {
		Local_service(Rom_session::service_name(),     &rom_root),
		Local_service(Ram_session::service_name(),     &ram_root),
		Local_service(Rm_session::service_name(),      &rm_root),
		Local_service(Cpu_session::service_name(),     &cpu_root),
		Local_service(Pd_session::service_name(),      &pd_root),
		Local_service(Log_session::service_name(),     &log_root),
		Local_service(Io_mem_session::service_name(),  &io_mem_root),
		Local_service(Irq_session::service_name(),     &irq_root),
		Local_service(Trace::Session::service_name(),  &trace_root)
	};

	/* make our local services known to service pool */
	for (unsigned i = 0; i < sizeof(ls) / sizeof(Local_service); i++)
		local_services.insert(&ls[i]);

	/* make platform-specific services known to service pool */
	platform_add_local_services(e, &sliced_heap, &local_services);

	/* obtain ROM session with init binary */
	Rom_session_capability init_rom_session_cap;
	try {
		static Rom_connection rom("init");
		init_rom_session_cap = rom.cap(); }
	catch (...) {
		error("ROM module \"init\" not present"); }

	/* create ram session for init and transfer some of our own quota */
	Ram_session_capability init_ram_session_cap
		= static_cap_cast<Ram_session>(ram_root.session("ram_quota=32K", Affinity()));
	Ram_session_client(init_ram_session_cap).ref_account(env()->ram_session_cap());

	/* create CPU session for init and transfer all of the CPU quota to it */
	static Cpu_session_component
		cpu(e, e, &pager_ep, &sliced_heap, Trace::sources(),
		    "label=\"core\"", Affinity(), Cpu_session::QUOTA_LIMIT);
	Cpu_session_capability cpu_cap = core_env()->entrypoint()->manage(&cpu);
	Cpu_connection init_cpu("init");
	init_cpu.ref_account(cpu_cap);
	cpu.transfer_quota(init_cpu, Cpu_session::quota_lim_upscale(100, 100));

	/* transfer all left memory to init, but leave some memory left for core */
	/* NOTE: exception objects thrown in core components are currently allocated on
	         core's heap and not accounted by the component's meta data allocator */
	Genode::size_t init_quota = platform()->ram_alloc()->avail() - 224*1024;
	env()->ram_session()->transfer_quota(init_ram_session_cap, init_quota);
	log("", init_quota / (1024*1024), " MiB RAM assigned to init");

	Pd_connection init_pd("init");
	Core_child *init = new (env()->heap())
		Core_child(Rom_session_client(init_rom_session_cap).dataspace(),
		           init_pd, init_ram_session_cap, init_cpu.cap(),
		           local_services);

	platform()->wait_for_exit();

	destroy(env()->heap(), init);

	rom_root.close(init_rom_session_cap);
	ram_root.close(init_ram_session_cap);

	return 0;
}
