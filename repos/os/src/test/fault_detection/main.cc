/*
 * \brief  Test program for detecting faults
 * \author Norman Feske
 * \date   2013-01-03
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/component.h>
#include <base/child.h>
#include <rom_session/connection.h>
#include <log_session/log_session.h>
#include <loader_session/connection.h>
#include <region_map/client.h>

using namespace Genode;


template <typename TEST>
class Iterative_test
{
	public:

	private:

		Env                      &_env;
		Signal_context_capability _finished_sigh;
		unsigned            const _cnt_max = 5;
		unsigned                  _cnt     = 0;

		Signal_handler<Iterative_test> _fault_handler {
			_env.ep(), *this, &Iterative_test::_handle_fault };

		TEST _test;;

		void _handle_fault()
		{
			if (_cnt++ >= _cnt_max) {
				Signal_transmitter(_finished_sigh).submit();
				log("-- finished ", _test.name(), " --");
				return;
			}
			_test.start_iteration(_env, _fault_handler);
		}

	public:

		Iterative_test(Env &env, Signal_context_capability finished_sigh)
		:
			_env(env), _finished_sigh(finished_sigh)
		{
			log("-- exercise ", _test.name(), " --");
			_test.start_iteration(_env, _fault_handler);
		}
};


/********************************************************************
 ** Test for detecting the failure of an immediate child component **
 ********************************************************************/

class Test_child : public Genode::Child_policy
{
	private:

		Env                      &_env;
		size_t              const _ram_quota = 1024*1024;
		Binary_name         const _binary_name;
		Signal_context_capability _sigh;
		Parent_service            _cpu_service { _env, Cpu_session::service_name() };
		Parent_service            _ram_service { _env, Ram_session::service_name() };
		Parent_service            _pd_service  { _env,  Pd_session::service_name() };
		Parent_service            _log_service { _env, Log_session::service_name() };
		Parent_service            _rom_service { _env, Rom_session::service_name() };
		Child                     _child;

	public:

		/**
		 * Constructor
		 */
		Test_child(Env &env, Name const &binary_name,
		           Genode::Signal_context_capability sigh)
		:
			_env(env), _binary_name(binary_name), _sigh(sigh),
			_child(_env.rm(), _env.ep().rpc_ep(), *this)
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "child"; }

		Binary_name binary_name() const override { return _binary_name; }

		Ram_session           &ref_ram()           override { return _env.ram(); }
		Ram_session_capability ref_ram_cap() const override { return _env.ram_session_cap(); }

		void init(Ram_session &ram, Ram_session_capability ram_cap) override
		{
			ram.ref_account(ref_ram_cap());
			ref_ram().transfer_quota(ram_cap, _ram_quota);
		}

		void init(Cpu_session &cpu, Cpu_session_capability) override
		{
			/* register default exception handler */
			cpu.exception_sigh(_sigh);
		}

		void init(Pd_session &pd, Pd_session_capability) override
		{
			/* register handler for unresolvable page faults */
			Region_map_client address_space(pd.address_space());
			address_space.fault_handler(_sigh);
		}

		Service &resolve_session_request(Service::Name const &service,
		                                 Session_state::Args const &args) override
		{
			if (service == Cpu_session::service_name()) return _cpu_service;
			if (service == Ram_session::service_name()) return _ram_service;
			if (service ==  Pd_session::service_name()) return  _pd_service;
			if (service == Log_session::service_name()) return _log_service;
			if (service == Rom_session::service_name()) return _rom_service;

			throw Parent::Service_denied();
		}
};


struct Faulting_child_test
{
	static char const *name() { return "failure detection in immediate child"; }

	Constructible<Test_child> _child;

	void start_iteration(Env &env, Signal_context_capability fault_sigh)
	{
		_child.construct(env, "test-segfault", fault_sigh);
	}
};


/******************************************************************
 ** Test for detecting failures in a child started by the loader **
 ******************************************************************/

struct Faulting_loader_child_test
{
	static char const *name() { return "failure detection in loaded child"; }

	Constructible<Loader::Connection> loader;

	void start_iteration(Env &env, Signal_context_capability fault_sigh)
	{
		loader.construct(env, 1024*1024);

		/* register fault handler at loader session */
		loader->fault_sigh(fault_sigh);

		/* start subsystem */
		loader->start("test-segfault");
	}
};


/***********************************************************************
 ** Test for detecting failures in a grandchild started by the loader **
 ***********************************************************************/

struct Faulting_loader_grand_child_test
{
	static char const *name() { return "failure detection of loaded grand child"; }

	static char const *config()
	{
		return
			"<config>\n"
			"  <parent-provides>\n"
			"    <service name=\"ROM\"/>\n"
			"    <service name=\"RAM\"/>\n"
			"    <service name=\"CPU\"/>\n"
			"    <service name=\"PD\"/>\n"
			"    <service name=\"LOG\"/>\n"
			"  </parent-provides>\n"
			"  <default-route>\n"
			"    <any-service> <parent/> <any-child/> </any-service>\n"
			"  </default-route>\n"
			"  <start name=\"test-segfault\">\n"
			"    <resource name=\"RAM\" quantum=\"10M\"/>\n"
			"  </start>\n"
			"</config>";
	}

	static size_t config_size() { return strlen(config()); }

	Constructible<Loader::Connection> loader;

	void start_iteration(Env &env, Signal_context_capability fault_sigh)
	{
		loader.construct(env, 2*1024*1024);

		/* import config into loader session */
		{
			Attached_dataspace ds(loader->alloc_rom_module("config", config_size()));
			memcpy(ds.local_addr<char>(), config(), config_size());
			loader->commit_rom_module("config");
		}

		/* register fault handler at loader session */
		loader->fault_sigh(fault_sigh);

		/* start subsystem */
		loader->start("init", "init");
	}
};


struct Main
{
	Env &_env;

	Constructible<Iterative_test<Faulting_child_test> >              _test_1;
	Constructible<Iterative_test<Faulting_loader_child_test> >       _test_2;
	Constructible<Iterative_test<Faulting_loader_grand_child_test> > _test_3;

	Signal_handler<Main> _test_1_finished_handler {
		_env.ep(), *this, &Main::_handle_test_1_finished };

	Signal_handler<Main> _test_2_finished_handler {
		_env.ep(), *this, &Main::_handle_test_2_finished };

	Signal_handler<Main> _test_3_finished_handler {
		_env.ep(), *this, &Main::_handle_test_3_finished };

	void _handle_test_1_finished()
	{
		_test_1.destruct();
		_test_2.construct(_env, _test_2_finished_handler);
	}

	void _handle_test_2_finished()
	{
		_test_2.destruct();
		_test_3.construct(_env, _test_3_finished_handler);
	}

	void _handle_test_3_finished()
	{
		_test_3.destruct();
		log("--- finished fault_detection test ---");
		_env.parent().exit(0);
	}

	Main(Env &env) : _env(env)
	{
		_test_1.construct(_env, _test_1_finished_handler);
	}
};


void Component::construct(Env &env) { static Main main(env); }

