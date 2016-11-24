/*
 * \brief  Test for yielding resources
 * \author Norman Feske
 * \date   2013-10-05
 *
 * This test exercises the protocol between a parent and child, which is used
 * by the parent to regain resources from a child subsystem.
 *
 * The program acts in either one of two roles, the parent or the child. The
 * role is determined by reading a config argument.
 *
 * The child periodically allocates chunks of RAM until its RAM quota is
 * depleted. Once it observes a yield request from the parent, however, it
 * cooperatively releases as much resources as requested by the parent.
 *
 * The parent wait a while to give the child the chance to allocate RAM. It
 * then sends a yield request and waits for a response. When getting the
 * response, it validates whether the child complied to the request or not.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/signal.h>
#include <timer_session/connection.h>
#include <os/slave.h>


/****************
 ** Child role **
 ****************/

/**
 * The child eats more and more RAM. However, when receiving a yield request,
 * it releases the requested amount of resources.
 */
class Child
{
	private:

		typedef Genode::size_t size_t;

		struct Ram_chunk : Genode::List<Ram_chunk>::Element
		{
			Genode::Env &env;

			size_t const size;

			Genode::Ram_dataspace_capability ds_cap;

			Ram_chunk(Genode::Env &env, size_t size)
			:
				env(env),size(size), ds_cap(env.ram().alloc(size))
			{ }

			~Ram_chunk() { env.ram().free(ds_cap); }
		};

		Genode::Env                  &_env;
		Genode::Heap                  _heap { _env.ram(), _env.rm() };
		bool                    const _expand;
		Genode::List<Ram_chunk>       _ram_chunks;
		Timer::Connection             _timer { _env };
		Genode::Signal_handler<Child> _periodic_timeout_handler;
		Genode::Signal_handler<Child> _yield_handler;
		unsigned long           const _period_ms;

		void _handle_periodic_timeout();
		void _handle_yield();

		void _schedule_next_timeout()
		{
			_timer.trigger_once(_period_ms*1000);
		}

	public:

		Child(Genode::Env &, Genode::Xml_node);
		void main();
};


void Child::_handle_periodic_timeout()
{
	size_t const chunk_size = 1024*1024;

	if (_env.ram().avail() < chunk_size) {

		if (_expand) {
			Genode::log("quota consumed, request additional resources");

			/*
			 * The attempt to allocate RAM will result in a resource request to
			 * the parent. The resource request will block until the parent
			 * responds.
			 */

		} else {
			Genode::log("consumed all of our quota, stop allocating");
			return;
		}
	}

	/* perform allocation and remember chunk in list */
	_ram_chunks.insert(new (_heap) Ram_chunk(_env, chunk_size));

	Genode::log("allocated chunk of ", chunk_size / 1024, " KiB");

	_schedule_next_timeout();
}


void Child::_handle_yield()
{
	using namespace Genode;

	/* request yield request arguments */
	Parent::Resource_args const args = _env.parent().yield_request();

	log("yield request: ", args.string());

	size_t const requested_ram_quota =
		Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

	/* free chunks of RAM to comply with the request */
	size_t released_quota = 0;
	while (released_quota < requested_ram_quota) {

		Ram_chunk *chunk = _ram_chunks.first();
		if (!chunk) {
			warning("no chunk left to release");
			break;
		}

		size_t const chunk_size = chunk->size;
		_ram_chunks.remove(chunk);
		destroy(_heap, chunk);
		released_quota += chunk_size;

		log("released chunk of ", chunk_size, " bytes");
	}

	/* acknowledge yield request */
	_env.parent().yield_response();

	_schedule_next_timeout();
}


Child::Child(Genode::Env &env, Genode::Xml_node config)
:
	_env(env),
	_expand(config.attribute_value("expand", false)),
	_periodic_timeout_handler(_env.ep(), *this, &Child::_handle_periodic_timeout),
	_yield_handler(_env.ep(), *this, &Child::_handle_yield),
	_period_ms(config.attribute_value("period_ms", 500UL))
{
	/* register yield signal handler */
	_env.parent().yield_sigh(_yield_handler);

	/* register timeout signal handler and schedule periodic timeouts */
	_timer.sigh(_periodic_timeout_handler);

	_schedule_next_timeout();
}


/*****************
 ** Parent role **
 *****************/

/**
 * The parent grants resource requests as long as it has free resources.
 * Once in a while, it politely requests the child to yield resources.
 */
class Parent
{
	private:

		Genode::Env &_env;

		typedef Genode::size_t size_t;

		Timer::Connection _timer { _env };

		Genode::Lock _yield_blockade;

		void _print_status()
		{
			Genode::log("quota: ", _child.ram().quota() / 1024, " KiB  "
			            "used: ",  _child.ram().used()  / 1024, " KiB");
		}

		size_t _used_ram_prior_yield = 0;

		/* perform the test three times */
		unsigned _cnt = 3;

		unsigned const _wait_secs = 5;

		unsigned _wait_cnt = 0;

		enum State { WAIT, YIELD_REQUESTED, YIELD_GOT_RESPONSE };
		State _state = WAIT;

		void _schedule_one_second_timeout()
		{
			Genode::log("wait ", _wait_cnt, "/", _wait_secs);
			_timer.trigger_once(1000*1000);
		}

		void _init()
		{
			_state = WAIT;
			_wait_cnt = 0;
			_schedule_one_second_timeout();
		}

		void _request_yield()
		{
			/* remember quantum of resources used by the child */
			_used_ram_prior_yield = _child.ram().used();

			Genode::log("request yield (ram prior yield: ", _used_ram_prior_yield);

			/* issue yield request */
			Genode::Parent::Resource_args yield_args("ram_quota=5M");
			_child.yield(yield_args);

			_state = YIELD_REQUESTED;
		}

		void _handle_timeout()
		{
			_print_status();
			_wait_cnt++;
			if (_wait_cnt >= _wait_secs) {
				_request_yield();
			} else {
				_schedule_one_second_timeout();
			}
		}

		void _yield_response()
		{
			Genode::log("got yield response");
			_state = YIELD_GOT_RESPONSE;

			_print_status();

			/* validate that the amount of yielded resources matches the request */
			size_t const used_after_yield = _child.ram().used();
			if (used_after_yield + 5*1024*1024 > _used_ram_prior_yield) {
				Genode::error("child has not yielded enough resources");
				throw Insufficient_yield();
			}

			if (_cnt-- > 0) {
				_init();
			} else {
				Genode::log("--- test-resource_yield finished ---");
				_env.parent().exit(0);
			}
		}

		Genode::Signal_handler<Parent> _timeout_handler {
			_env.ep(), *this, &Parent::_handle_timeout };

		struct Policy : Genode::Slave::Policy
		{
			Parent &_parent;

			enum { SLAVE_QUOTA = 10*1024*1024 };

			char const **_permitted_services() const override
			{
				static char const *services[] = { "RAM", "PD", "CPU", "ROM", "LOG", "Timer" };
				return services;
			}

			void yield_response() override
			{
				_parent._yield_response();
			}

			Policy(Parent &parent, Genode::Env &env)
			:
				Genode::Slave::Policy(Label("child"), "test-resource_yield",
				                      env.ep().rpc_ep(), env.rm(),
				                      env.ram_session_cap(), SLAVE_QUOTA),
				_parent(parent)
			{
				configure("<config child=\"yes\" />");
			}
		};

		Policy _policy { *this, _env };

		Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), _policy };

	public:

		class Insufficient_yield { };

		/**
		 * Constructor
		 */
		Parent(Genode::Env &env) : _env(env)
		{
			_timer.sigh(_timeout_handler);
			_init();
		}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	/*
	 * Read value '<config child="" />' attribute to decide whether to perform
	 * the child or the parent role.
	 */
	static Attached_rom_dataspace config(env, "config");
	bool const is_child = config.xml().attribute_value("child", false);

	if (is_child) {
		log("--- test-resource_yield child role started ---");
		static ::Child child(env, config.xml());
	} else {
		log("--- test-resource_yield parent role started ---");
		static ::Parent parent(env);
	}
}
