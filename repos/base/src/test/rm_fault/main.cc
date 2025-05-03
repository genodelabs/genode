/*
 * \brief  Test program for raising and handling region-manager faults
 * \author Norman Feske
 * \date   2008-09-24
 *
 * This program starts itself as child. When started, it first determines
 * whether it is parent or child by requesting a RM session. Because the
 * program blocks all session-creation calls for the RM service, each program
 * instance can determine its parent or child role by the checking the result
 * of the session creation.
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/child.h>
#include <base/sleep.h>
#include <rm_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>

using namespace Genode;


/***********
 ** Child **
 ***********/

enum {
	MANAGED_ADDR = 0x18000000,
	STOP_TEST    = 0xdead,
	READ_TEST    = 0x12345,
	WRITE_TEST   = READ_TEST - 1,
	EXEC_TEST    = WRITE_TEST - 1,
	SHUTDOWN     = EXEC_TEST  - 1
};

static char const *fault_name(Region_map::Fault const &fault)
{
	return fault.type == Region_map::Fault::Type::READ  ? "READ_FAULT"  :
	       fault.type == Region_map::Fault::Type::WRITE ? "WRITE_FAULT" :
	       fault.type == Region_map::Fault::Type::EXEC  ? "EXEC_FAULT"  : "READY";
}


void read_at(addr_t addr)
{
	log("perform read operation at ", Hex(addr));
	int value = *(int *)addr;
	log("read value ", Hex(value));
}

bool modify_at(addr_t addr)
{
	addr_t const value = *(addr_t volatile *)addr;

	if (value == STOP_TEST)
		return false;

	if (value != READ_TEST + 1) {

		(*(addr_t volatile *)(addr)) = (*(addr_t volatile *)(addr)) + 1;
		addr_t value_mod = (*(addr_t volatile *)(addr));

		/* if we are get told to stop, do so */
		if (*(addr_t volatile *)(addr + sizeof(addr)) == STOP_TEST)
			return false;

		log("modify memory at ", Hex(addr), " from ",
		    Hex(value), " to ", Hex(value_mod));
	}

	if (value != READ_TEST && value != READ_TEST + 1)
	{
		Genode::error("could modify ROM !!! ", Hex(value));
		return false;
	}

	return true;
}

struct Exec_faulter : Thread
{
	enum { FAULT_ON_ADDR, FAULT_ON_STACK };

	unsigned _fault_test;

	Exec_faulter(Env &env, unsigned test)
	: Thread(env, "exec_fault", 1024 * sizeof(addr_t)), _fault_test(test)
	{ }

	void entry() override
	{
		if (_fault_test == FAULT_ON_ADDR) {
			addr_t volatile * value = (addr_t volatile *)MANAGED_ADDR;
			*value = 0x0b0f9090; /* nop, nop, ud2 */

			void (*exec_fault)(void) = (void (*)(void))MANAGED_ADDR;
			exec_fault();
			return;
		}

		if (_fault_test == FAULT_ON_STACK) {
			unsigned long dummy = 0x0b0f9090; /* nop, nop, ud2 */

			void (*exec_fault)(void) = (void (*)(void))&dummy;
			exec_fault();
		}
	}
};

void execute_at(Genode::Env &env, Attached_rom_dataspace &config, addr_t cmd_addr)
{
	addr_t volatile * cmd = (addr_t volatile *)cmd_addr;

	if (config.xml().attribute_value("executable_fault_test", true)) {
		/* perform illegal execute access on cmd addr */
		Exec_faulter fault_on_managed_addr(env, Exec_faulter::FAULT_ON_ADDR);
		fault_on_managed_addr.start();

		/* wait until parent acknowledged fault */
		while (*cmd != STOP_TEST) { }

		/* tell parent that we start with next EXEC_TEST */
		*cmd = EXEC_TEST;

		Exec_faulter fault_on_stack(env, Exec_faulter::FAULT_ON_STACK);
		fault_on_stack.start();

		/* wait until parent acknowledged fault */
		while (*cmd == EXEC_TEST) { }
	}

	log("\n--- child role of region-manager fault test finished ---");

	/* sync shutdown with parent */
	*cmd = SHUTDOWN;
}

void main_child(Env &env)
{
	Attached_rom_dataspace config { env, "config" };

	log("child role started");

	/* perform illegal read access */
	read_at(MANAGED_ADDR);

	/* perform illegal write access */
	while (modify_at(MANAGED_ADDR));

	/* perform illegal execute access */
	execute_at(env, config, MANAGED_ADDR);
}


/************
 ** Parent **
 ************/

class Test_child_policy : public Child_policy
{
	public:

		using Parent_service  = Registered<Genode::Parent_service>;
		using Parent_services = Registry<Parent_service>;

	private:

		Env                            &_env;
		Parent_services                &_parent_services;
		Id_space<Parent::Server>        _server_ids { };
		Signal_context_capability const _fault_handler_sigh;
		Signal_context_capability const _fault_handler_stack_sigh;

		void _with_matching_service(Service::Name const &name,
		                            auto const &fn, auto const &denied_fn)
		{
			Service *service_ptr = nullptr;
			_parent_services.for_each([&] (Service &s) {
				if (!service_ptr && name == s.name())
					service_ptr = &s; });

			if (service_ptr) fn(*service_ptr); else denied_fn();
		}

	public:

		/**
		 * Constructor
		 */
		Test_child_policy(Env &env, Parent_services &parent_services,
		                  Signal_context_capability fault_handler_sigh,
		                  Signal_context_capability fault_handler_stack_sigh)
		:
			_env(env),
			_parent_services(parent_services),
			_fault_handler_sigh(fault_handler_sigh),
			_fault_handler_stack_sigh(fault_handler_stack_sigh)
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "rmchild"; }

		Binary_name binary_name() const override { return "test-rm_fault"; }

		Ram_allocator &session_md_ram() override { return _env.ram(); }

		Pd_account            &ref_account()           override { return _env.pd(); }
		Capability<Pd_account> ref_account_cap() const override { return _env.pd_session_cap(); }

		void init(Pd_session &session, Pd_session_capability cap) override
		{
			session.ref_account(_env.pd_session_cap());

			_env.pd().transfer_quota(cap, Ram_quota{1*1024*1024});
			_env.pd().transfer_quota(cap, Cap_quota{20});

			Region_map_client address_space(session.address_space());
			address_space.fault_handler(_fault_handler_sigh);

			Region_map_client stack_area(session.stack_area());
			stack_area.fault_handler(_fault_handler_stack_sigh);
		}

		void _with_route(Service::Name     const &name,
		                 Session_label     const &label,
		                 Session::Diag     const  diag,
		                 With_route::Ft    const &fn,
		                 With_no_route::Ft const &) override
		{
			_with_matching_service(name, [&] (Service &service) {
				fn(Route { .service = service,
				           .label   = label,
				           .diag    = diag });
			}, [&] { });
		}

		Id_space<Parent::Server> &server_id_space() override { return _server_ids; }
};


struct Main_parent
{
	Env &_env;

	Signal_handler<Main_parent> _fault_handler {
		_env.ep(), *this, &Main_parent::_handle_fault };

	Signal_handler<Main_parent> _fault_handler_stack {
		_env.ep(), *this, &Main_parent::_handle_fault_stack };

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };
	Rom_connection _binary { _env, "ld.lib.so" };

	/* parent services */
	struct Parent_services : Test_child_policy::Parent_services
	{
		Allocator &alloc;

		Parent_services(Env &env, Allocator &alloc) : alloc(alloc)
		{
			static const char *names[] = {
				"PD", "CPU", "ROM", "LOG", 0 };
			for (unsigned i = 0; names[i]; i++)
				new (alloc) Test_child_policy::Parent_service(*this, env, names[i]);
		}

		~Parent_services()
		{
			for_each([&] (Test_child_policy::Parent_service &s) { destroy(alloc, &s); });
		}
	} _parent_services { _env, _heap };

	/* create child */
	Test_child_policy _child_policy { _env, _parent_services, _fault_handler,
	                                  _fault_handler_stack };

	Child _child { _env.rm(), _env.ep().rpc_ep(), _child_policy };

	Pd_session &_child_pd()
	{
		Pd_session *pd_ptr = nullptr;
		_child.with_pd([&] (Pd_session &pd) { pd_ptr = &pd; }, [&] { });
		if (!pd_ptr) {
			error("child PD unexpectedly uninitialized");
			sleep_forever();
		}
		return *pd_ptr;
	}

	Region_map_client _address_space { _child_pd().address_space() };

	/* dataspace used for creating shared memory between parent and child */
	Attached_ram_dataspace _ds { _env.ram(), _env.rm(), 4096 };

	unsigned _fault_cnt = 0;

	long volatile &_child_value() { return *_ds.local_addr<long volatile>(); }
	long volatile &_child_stop()  { return *(_ds.local_addr<long volatile>() + 1); }

	void _attach_at(Dataspace_capability ds, addr_t at)
	{
		if (_address_space.attach(ds, {
			.size       = { },   .offset    = { },
			.use_at     = true,  .at        = at,
			.executable = { },   .writeable = true
		}).failed()) error("_attach_at unexpectedly failed");
	}

	void _test_read_fault(addr_t const child_virt_addr)
	{
		/* allocate dataspace to resolve the fault */
		log("attach dataspace to the child at ", Hex(child_virt_addr));

		_child_value() = READ_TEST;

		_attach_at(_ds.cap(), child_virt_addr);

		/* poll until our child modifies the dataspace content */
		while (_child_value() == READ_TEST);

		log("child modified dataspace content, new value is ",
		    Hex(_child_value()));

		log("revoke dataspace from child");
		_address_space.detach(child_virt_addr);
	}

	void _test_write_fault(addr_t const child_virt_addr, unsigned round)
	{
		if (_child_value() != WRITE_TEST) {
			Genode::log("test WRITE faults on read-only binary and "
			            "read-only attached RAM");

			_child_value() = WRITE_TEST;

			_attach_at(_binary.dataspace(), child_virt_addr);
			return;
		}

		enum { ROUND_FAULT_ON_ROM_BINARY = 1, ROUND_FAULT_ON_RO_RAM = 2 };

		if (round == ROUND_FAULT_ON_RO_RAM)
			_child_stop() = STOP_TEST;

		Genode::log("got write fault on ", Hex(child_virt_addr),
		            (round == ROUND_FAULT_ON_ROM_BINARY) ? " ROM (binary)" :
		            (round == ROUND_FAULT_ON_RO_RAM) ? " read-only attached RAM"
		                                             : " unknown");

		/* detach region where fault happened */
		_address_space.detach(child_virt_addr);

		if (round == ROUND_FAULT_ON_ROM_BINARY) {
			/* attach a RAM dataspace read-only */
			if (_address_space.attach(_ds.cap(), {
				.size       = 4096,  .offset    = { },
				.use_at     = true,  .at        = child_virt_addr,
				.executable = { },   .writeable = { }
			}).failed()) error("attach of ROUND_FAULT_ON_ROM_BINARY failed");

		} else
		if (round == ROUND_FAULT_ON_RO_RAM) {
			/* let client continue by attaching RAM dataspace writeable */
			_attach_at(_ds.cap(), child_virt_addr);
		}
	}

	void _test_exec_fault(Region_map::Fault const &fault)
	{
		if (_child_value() == WRITE_TEST) {
			_child_value() = EXEC_TEST;
			return;
		}

		if (fault.type != Region_map::Fault::Type::EXEC ||
		    fault.addr != MANAGED_ADDR)
		{
			error("exec test failed ", (int)fault.type,
			      " addr=", Hex(fault.addr));
			return;
		}

		log("got exec fault on dataspace");
		/* signal client to continue with next test, current test is done */
		_child_value() = STOP_TEST;
	}

	void _handle_fault()
	{
		enum { FAULT_CNT_READ = 4, FAULT_CNT_WRITE = 6 };

		log("received region-map fault signal, request fault state");

		Region_map::Fault const fault = _address_space.fault();

		log("rm session state is ", fault_name(fault), ", pf_addr=", Hex(fault.addr));

		/* ignore spurious fault signal */
		if (fault.type == Region_map::Fault::Type::NONE) {
			log("ignoring spurious fault signal");
			return;
		}

		addr_t child_virt_addr = fault.addr & ~(4096 - 1);

		if (_fault_cnt < FAULT_CNT_READ)
			_test_read_fault(child_virt_addr);

		if (_fault_cnt <= FAULT_CNT_WRITE && _fault_cnt >= FAULT_CNT_READ)
			_test_write_fault(child_virt_addr, _fault_cnt - FAULT_CNT_READ);

		if (!_config.xml().attribute_value("executable_fault_test", true) &&
		    _fault_cnt >=FAULT_CNT_WRITE)
			_handle_fault_stack();

		if (_fault_cnt > FAULT_CNT_WRITE)
			_test_exec_fault(fault);

		_fault_cnt++;
	}

	void _handle_fault_stack()
	{
		/* sanity check that we got exec fault */
		if (_config.xml().attribute_value("executable_fault_test", true)) {
			Region_map::Fault const fault = _address_space.fault();
			if (fault.type != Region_map::Fault::Type::EXEC) {
				error("unexpected state ", fault_name(fault));
				return;
			}

			_child_value() = STOP_TEST;
		}

		/* sync shutdown with client */
		while (_child_value() != SHUTDOWN) { }

		log("--- parent role of region-manager fault test finished --- ");

		/* done, finally */
		_env.parent().exit(0);
	}

	Main_parent(Env &env) : _env(env) { }
};


void Component::construct(Env &env)
{
	log("--- region-manager fault test ---");

	try {
		/*
		 * Distinguish parent from child by requesting an service that is only
		 * available to the parent.
		 */
		Rm_connection rm(env);
		static Main_parent parent(env);
		log("-- parent role started --");
	}
	catch (Service_denied) {
		main_child(env);
	}
}
