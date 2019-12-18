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
	private:

		Env                      &_env;
		Signal_context_capability _finished_sigh;
		unsigned            const _cnt_max = 5;
		unsigned                  _cnt     = 0;

		Signal_handler<Iterative_test> _fault_handler {
			_env.ep(), *this, &Iterative_test::_handle_fault };

		TEST _test { };

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
		Cap_quota           const _cap_quota { 50 };
		Ram_quota           const _ram_quota { 1024*1024 };
		Binary_name         const _binary_name;
		Signal_context_capability _sigh;
		Parent_service            _cpu_service { _env, Cpu_session::service_name() };
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

		Pd_session           &ref_pd()           override { return _env.pd(); }
		Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

		void init(Cpu_session &cpu, Cpu_session_capability) override
		{
			/* register default exception handler */
			cpu.exception_sigh(_sigh);
		}

		void init(Pd_session &pd, Pd_session_capability pd_cap) override
		{
			pd.ref_account(ref_pd_cap());
			ref_pd().transfer_quota(pd_cap, _cap_quota);
			ref_pd().transfer_quota(pd_cap, _ram_quota);

			/* register handler for unresolvable page faults */
			Region_map_client address_space(pd.address_space());
			address_space.fault_handler(_sigh);
		}

		Route resolve_session_request(Service::Name const &service,
		                              Session_label const &label) override
		{
			auto route = [&] (Service &service) {
				return Route { .service = service,
				               .label   = label,
				               .diag    = Session::Diag() }; };

			if (service == Cpu_session::service_name()) return route(_cpu_service);
			if (service ==  Pd_session::service_name()) return route( _pd_service);
			if (service == Log_session::service_name()) return route(_log_service);
			if (service == Rom_session::service_name()) return route(_rom_service);

			throw Service_denied();
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
		loader.construct(env, Ram_quota{1024*1024}, Cap_quota{100});

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
			"  <start name=\"test-segfault\" caps=\"50\">\n"
			"    <resource name=\"RAM\" quantum=\"2M\"/>\n"
			"  </start>\n"
			"</config>";
	}

	static size_t config_size() { return strlen(config()); }

	Constructible<Loader::Connection> loader;

	void start_iteration(Env &env, Signal_context_capability fault_sigh)
	{
		loader.construct(env, Ram_quota{4*1024*1024}, Cap_quota{120});

		/* import config into loader session */
		{
			Attached_dataspace ds(env.rm(),
			                      loader->alloc_rom_module("config", config_size()));
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

	Constructible<Iterative_test<Faulting_child_test> >              _test_1 { };
	Constructible<Iterative_test<Faulting_loader_child_test> >       _test_2 { };
	Constructible<Iterative_test<Faulting_loader_grand_child_test> > _test_3 { };

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

