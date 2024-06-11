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
		Id_space<Parent::Server>  _server_ids { };
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
		                              Session_label const &label,
		                              Session::Diag const  diag) override
		{
			auto route = [&] (Service &service) {
				return Route { .service = service,
				               .label   = label,
				               .diag    = diag }; };

			if (service == Cpu_session::service_name()) return route(_cpu_service);
			if (service ==  Pd_session::service_name()) return route( _pd_service);
			if (service == Log_session::service_name()) return route(_log_service);
			if (service == Rom_session::service_name()) return route(_rom_service);

			throw Service_denied();
		}

		Id_space<Parent::Server> &server_id_space() override { return _server_ids; }
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


struct Main
{
	Env &_env;

	Constructible<Iterative_test<Faulting_child_test> > _test { };

	Signal_handler<Main> _test_finished_handler {
		_env.ep(), *this, &Main::_handle_test_finished };

	void _handle_test_finished()
	{
		_test.destruct();
		log("--- finished fault_detection test ---");
		_env.parent().exit(0);
	}

	Main(Env &env) : _env(env)
	{
		_test.construct(_env, _test_finished_handler);
	}
};


void Component::construct(Env &env) { static Main main(env); }

