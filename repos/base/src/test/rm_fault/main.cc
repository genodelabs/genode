/*
 * \brief  Test program for raising and handling region-manager faults
 * \author Norman Feske
 * \date   2008-09-24
 *
 * This program starts itself as child. When started, it first determines
 * wheather it is parent or child by requesting a RM session. Because the
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
#include <rm_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>

using namespace Genode;


/***********
 ** Child **
 ***********/

enum {
	MANAGED_ADDR = 0x10000000,
	STOP_TEST    = 0xdead,
	READ_TEST    = 0x12345,
	WRITE_TEST   = READ_TEST - 1,
	EXEC_TEST    = WRITE_TEST - 1,
	SHUTDOWN     = EXEC_TEST  - 1
};

static char const * const state_name(Region_map::State &state)
{
	return state.type == Region_map::State::READ_FAULT  ? "READ_FAULT"  :
	       state.type == Region_map::State::WRITE_FAULT ? "WRITE_FAULT" :
	       state.type == Region_map::State::EXEC_FAULT  ? "EXEC_FAULT"  : "READY";
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
		addr_t value_mod = ++(*(addr_t volatile *)(addr));

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

	void entry()
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

		typedef Registered<Genode::Parent_service> Parent_service;
		typedef Registry<Parent_service>           Parent_services;

	private:

		Env                            &_env;
		Parent_services                &_parent_services;
		Signal_context_capability const _fault_handler_sigh;
		Signal_context_capability const _fault_handler_stack_sigh;

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

		Pd_session           &ref_pd()           override { return _env.pd(); }
		Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

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

		Service &resolve_session_request(Service::Name const &service_name,
		                                 Session_state::Args const &args) override
		{
			Service *service = nullptr;
			_parent_services.for_each([&] (Service &s) {
				if (!service && service_name == s.name())
					service = &s; });

			if (!service)
				throw Service_denied();

			return *service;
		}
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
	Rom_connection _binary { _env, "test-rm_fault" };

	/* parent services */
	struct Parent_services : Test_child_policy::Parent_services
	{
		Allocator &alloc;

		Parent_services(Allocator &alloc) : alloc(alloc)
		{
			static const char *names[] = {
				"PD", "CPU", "ROM", "LOG", 0 };
			for (unsigned i = 0; names[i]; i++)
				new (alloc) Test_child_policy::Parent_service(*this, names[i]);
		}

		~Parent_services()
		{
			for_each([&] (Test_child_policy::Parent_service &s) { destroy(alloc, &s); });
		}
	} _parent_services { _heap };

	/* create child */
	Test_child_policy _child_policy { _env, _parent_services, _fault_handler,
	                                  _fault_handler_stack };

	Child _child { _env.rm(), _env.ep().rpc_ep(), _child_policy };

	Region_map_client _address_space { _child.pd().address_space() };

	/* dataspace used for creating shared memory between parent and child */
	Attached_ram_dataspace _ds { _env.ram(), _env.rm(), 4096 };

	unsigned _fault_cnt = 0;

	long volatile &_child_value() { return *_ds.local_addr<long volatile>(); }
	long volatile &_child_stop()  { return *(_ds.local_addr<long volatile>() + 1); }

	void _test_read_fault(addr_t const child_virt_addr)
	{
		/* allocate dataspace to resolve the fault */
		log("attach dataspace to the child at ", Hex(child_virt_addr));

		_child_value() = READ_TEST;

		_address_space.attach_at(_ds.cap(), child_virt_addr);

		/* poll until our child modifies the dataspace content */
		while (_child_value() == READ_TEST);

		log("child modified dataspace content, new value is ",
		    Hex(_child_value()));

		log("revoke dataspace from child");
		_address_space.detach((void *)child_virt_addr);
	}

	void _test_write_fault(addr_t const child_virt_addr)
	{
		if (_child_value() == WRITE_TEST) {
			_child_stop() = STOP_TEST;

			Genode::log("got write fault on ROM ", Hex(child_virt_addr));

			/* let client continue by providing a dataspace it may write to */
			_address_space.detach((void *)child_virt_addr);
			_address_space.attach_at(_ds.cap(), child_virt_addr);

			return;
		}

		Genode::log("test WRITE fault on read-only binary");

		_child_value() = WRITE_TEST;

		_address_space.attach_at(_binary.dataspace(), child_virt_addr);
	}

	void _test_exec_fault(addr_t const child_virt_addr, Region_map::State &state)
	{
		if (_child_value() == WRITE_TEST) {
			_child_value() = EXEC_TEST;
			return;
		}

		if (state.type != Region_map::State::EXEC_FAULT ||
		    state.addr != MANAGED_ADDR)
		{
			error("exec test failed ", (int)state.type,
			      " addr=", Hex(state.addr));
			return;
		}

		log("got exec fault on dataspace");
		/* signal client to continue with next test, current test is done */
		_child_value() = STOP_TEST;
	}

	void _handle_fault()
	{
		enum { FAULT_CNT_READ = 4, FAULT_CNT_WRITE = 5 };

		log("received region-map fault signal, request fault state");

		Region_map::State state = _address_space.state();

		log("rm session state is ", state_name(state), ", pf_addr=", Hex(state.addr));

		/* ignore spurious fault signal */
		if (state.type == Region_map::State::READY) {
			log("ignoring spurious fault signal");
			return;
		}

		addr_t child_virt_addr = state.addr & ~(4096 - 1);

		if (_fault_cnt < FAULT_CNT_READ)
			_test_read_fault(child_virt_addr);

		if (_fault_cnt <= FAULT_CNT_WRITE && _fault_cnt >= FAULT_CNT_READ)
			_test_write_fault(child_virt_addr);

		if (!_config.xml().attribute_value("executable_fault_test", true) &&
		    _fault_cnt >=FAULT_CNT_WRITE)
			_handle_fault_stack();

		if (_fault_cnt > FAULT_CNT_WRITE)
			_test_exec_fault(child_virt_addr, state);

		_fault_cnt++;
	}

	void _handle_fault_stack()
	{
		/* sanity check that we got exec fault */
		if (_config.xml().attribute_value("executable_fault_test", true)) {
			Region_map::State state = _address_space.state();
			if (state.type != Region_map::State::EXEC_FAULT) {
				error("unexpected state ", state_name(state));
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
