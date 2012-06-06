/*
 * \brief  Core main program
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/snprintf.h>
#include <base/sleep.h>
#include <base/service.h>
#include <base/child.h>
#include <rom_session/connection.h>
#include <cpu_session/connection.h>

/* core includes */
#include <platform.h>
#include <core_env.h>
#include <ram_root.h>
#include <rom_root.h>
#include <cap_root.h>
#include <rm_root.h>
#include <cpu_root.h>
#include <pd_root.h>
#include <log_root.h>
#include <io_mem_root.h>
#include <io_port_root.h>
#include <irq_root.h>
#include <signal_root.h>

using namespace Genode;


/* support for cap session component */
long Cap_session_component::_unique_id_cnt;

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


Env * Genode::env() {
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
                                        Parent::Session_args const &args)
{
	Service *service = local_services.find(name.string());

	if (service)
		return service->session(args.string());

	PWRN("service_name=\"%s\" arg=\"%s\" not handled", name.string(), args.string());
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
		enum { STACK_SIZE = 8*1024 };

		Child _child;

		Service_registry *_local_services;

	public:

		/**
		 * Constructor
		 */
		Core_child(Dataspace_capability elf_ds, Cap_session *cap_session,
		           Ram_session_capability ram, Cpu_session_capability cpu,
		           Rm_session_capability rm, Service_registry *services)
		:
			_entrypoint(cap_session, STACK_SIZE, "init", false),
			_child(elf_ds, ram, cpu, rm, &_entrypoint, this),
			_local_services(services)
		{
			_entrypoint.activate();
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		const char *name() const { return "init"; }

		Service *resolve_session_request(const char *service, const char *)
		{
			return _local_services->find(service);
		}
};


/***************
 ** Core main **
 ***************/

int main()
{
	PDBG("--- create local services ---");

	/*
	 * Initialize root interfaces for our services
	 */
	Rpc_entrypoint *e = core_env()->entrypoint();

	/*
	 * Allocate session meta data on distinct dataspaces to enable independent
	 * destruction (to enable quota trading) of session component objects.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	static Cap_root     cap_root     (e, &sliced_heap);
	static Ram_root     ram_root     (e, e, platform()->ram_alloc(), &sliced_heap);
	static Rom_root     rom_root     (e, e, platform()->rom_fs(), &sliced_heap);
	static Rm_root      rm_root      (e, e, e, &sliced_heap, core_env()->cap_session(),
	                                  platform()->vm_start(), platform()->vm_size());
	static Cpu_root     cpu_root     (e, e, rm_root.pager_ep(), &sliced_heap);
	static Pd_root      pd_root      (e, e, &sliced_heap);
	static Log_root     log_root     (e, &sliced_heap);
	static Io_mem_root  io_mem_root  (e, e, platform()->io_mem_alloc(),
	                                  platform()->ram_alloc(), &sliced_heap);
	static Io_port_root io_port_root (core_env()->cap_session(), platform()->io_port_alloc(), &sliced_heap);
	static Irq_root     irq_root     (core_env()->cap_session(),
	                                  platform()->irq_alloc(), &sliced_heap);
	static Signal_root  signal_root  (&sliced_heap, core_env()->cap_session());

	/*
	 * Play our role as parent of init and declare our services.
	 */

	static Local_service ls[] = {
		Local_service(Rom_session::service_name(),     &rom_root),
		Local_service(Ram_session::service_name(),     &ram_root),
		Local_service(Cap_session::service_name(),     &cap_root),
		Local_service(Rm_session::service_name(),      &rm_root),
		Local_service(Cpu_session::service_name(),     &cpu_root),
		Local_service(Pd_session::service_name(),      &pd_root),
		Local_service(Log_session::service_name(),     &log_root),
		Local_service(Io_mem_session::service_name(),  &io_mem_root),
		Local_service(Io_port_session::service_name(), &io_port_root),
		Local_service(Irq_session::service_name(),     &irq_root),
		Local_service(Signal_session::service_name(),  &signal_root)
	};

	/* make our local services known to service pool */
	for (unsigned i = 0; i < sizeof(ls) / sizeof(Local_service); i++)
		local_services.insert(&ls[i]);

	PDBG("--- start init ---");

	/* obtain ROM session with init binary */
	Rom_session_capability init_rom_session_cap;
	try {
		static Rom_connection rom("init");
		init_rom_session_cap = rom.cap(); }
	catch (...) {
		PERR("ROM module \"init\" not present"); }

	/* create ram session for init and transfer some of our own quota */
	Ram_session_capability init_ram_session_cap
		= static_cap_cast<Ram_session>(ram_root.session("ram_quota=32K"));
	Ram_session_client(init_ram_session_cap).ref_account(env()->ram_session_cap());

	Cpu_connection init_cpu;
	Rm_connection  init_rm;

	/* transfer all left memory to init, but leave some memory left for core */
	/* NOTE: exception objects thrown in core components are currently allocated on
	         core's heap and not accounted by the component's meta data allocator */
	size_t init_quota = platform()->ram_alloc()->avail() - 72*1024;
	env()->ram_session()->transfer_quota(init_ram_session_cap, init_quota);
	PDBG("transferred %zd MB to init", init_quota / (1024*1024));

	Core_child *init = new (env()->heap())
		Core_child(Rom_session_client(init_rom_session_cap).dataspace(),
		           core_env()->cap_session(), init_ram_session_cap,
		           init_cpu.cap(), init_rm.cap(), &local_services);

	PDBG("--- init created, waiting for exit condition ---");
	platform()->wait_for_exit();

	PDBG("--- destroying init ---");
	destroy(env()->heap(), init);

	rom_root.close(init_rom_session_cap);
	ram_root.close(init_ram_session_cap);

	PDBG("--- core main says good bye ---");

	return 0;
}
