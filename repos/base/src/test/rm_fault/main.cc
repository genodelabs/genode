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

using namespace Genode;


/***********
 ** Child **
 ***********/

enum { MANAGED_ADDR = 0x10000000 };


void read_at(addr_t addr)
{
	log("perform read operation at ", Hex(addr));
	int value = *(int *)addr;
	log("read value ", Hex(value));
}

void modify(addr_t addr)
{
	log("modify memory at ", Hex(addr), " to ", Hex(++(*(int *)addr)));
}

void main_child()
{
	log("child role started");

	/* perform illegal access */
	read_at(MANAGED_ADDR);

	while (true)
		modify(MANAGED_ADDR);

	log("--- child role of region-manager fault test finished ---");
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

	public:

		/**
		 * Constructor
		 */
		Test_child_policy(Env &env, Parent_services &parent_services,
		                  Signal_context_capability fault_handler_sigh)
		:
			_env(env),
			_parent_services(parent_services),
			_fault_handler_sigh(fault_handler_sigh)
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "rmchild"; }

		Binary_name binary_name() const override { return "test-rm_fault"; }

		Ram_session &ref_ram() override { return _env.ram(); }

		Ram_session_capability ref_ram_cap() const override { return _env.ram_session_cap(); }

		void init(Ram_session &session, Ram_session_capability cap) override
		{
			enum { CHILD_QUOTA = 1*1024*1024 };
			session.ref_account(_env.ram_session_cap());
			_env.ram().transfer_quota(cap, CHILD_QUOTA);
		}

		void init(Pd_session &session, Pd_session_capability cap) override
		{
			Region_map_client address_space(session.address_space());
			address_space.fault_handler(_fault_handler_sigh);
		}

		Service &resolve_session_request(Service::Name const &service_name,
		                                 Session_state::Args const &args) override
		{
			Service *service = nullptr;
			_parent_services.for_each([&] (Service &s) {
				if (!service && service_name == s.name())
					service = &s; });

			if (!service)
				throw Parent::Service_denied();

			return *service;
		}
};


struct Main_parent
{
	Env &_env;

	Signal_handler<Main_parent> _fault_handler {
		_env.ep(), *this, &Main_parent::_handle_fault };

	Heap _heap { _env.ram(), _env.rm() };

	/* parent services */
	struct Parent_services : Test_child_policy::Parent_services
	{
		Allocator &alloc;

		Parent_services(Allocator &alloc) : alloc(alloc)
		{
			static const char *names[] = {
				"RAM", "PD", "CPU", "ROM", "LOG", 0 };
			for (unsigned i = 0; names[i]; i++)
				new (alloc) Test_child_policy::Parent_service(*this, names[i]);
		}

		~Parent_services()
		{
			for_each([&] (Test_child_policy::Parent_service &s) { destroy(alloc, &s); });
		}
	} _parent_services { _heap };

	/* create child */
	Test_child_policy _child_policy { _env, _parent_services, _fault_handler };

	Child _child { _env.rm(), _env.ep().rpc_ep(), _child_policy };

	Region_map_client _address_space { _child.pd().address_space() };

	/* dataspace used for creating shared memory between parent and child */
	Attached_ram_dataspace _ds { _env.ram(), _env.rm(), 4096 };

	unsigned _fault_cnt = 0;

	long volatile &_child_value() { return *_ds.local_addr<long volatile>(); }

	void _handle_fault()
	{
		if (_fault_cnt++ == 4) {
			log("--- parent role of region-manager fault test finished ---");
			_env.parent().exit(0);
		}

		log("received region-map fault signal, request fault state");

		Region_map::State state = _address_space.state();

		char const *state_name =
			state.type == Region_map::State::READ_FAULT  ? "READ_FAULT"  :
			state.type == Region_map::State::WRITE_FAULT ? "WRITE_FAULT" :
			state.type == Region_map::State::EXEC_FAULT  ? "EXEC_FAULT"  : "READY";

		log("rm session state is ", state_name, ", pf_addr=", Hex(state.addr));

		/* ignore spuriuous fault signal */
		if (state.type == Region_map::State::READY) {
			log("ignoring spurious fault signal");
			return;
		}

		addr_t child_virt_addr = state.addr & ~(4096 - 1);

		/* allocate dataspace to resolve the fault */
		log("attach dataspace to the child at ", Hex(child_virt_addr));
		_child_value() = 0x1234;

		_address_space.attach_at(_ds.cap(), child_virt_addr);

		/* poll until our child modifies the dataspace content */
		while (_child_value() == 0x1234);

		log("child modified dataspace content, new value is ",
		    Hex(_child_value()));

		log("revoke dataspace from child");
		_address_space.detach((void *)child_virt_addr);
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
	catch (Parent::Service_denied) {
		main_child();
	}
}
