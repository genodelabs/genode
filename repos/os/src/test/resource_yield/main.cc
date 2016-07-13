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
#include <base/log.h>
#include <base/signal.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>
#include <os/config.h>
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
			size_t const size;

			Genode::Ram_dataspace_capability ds_cap;

			Ram_chunk(size_t size)
			:
				size(size),
				ds_cap(Genode::env()->ram_session()->alloc(size))
			{ }

			~Ram_chunk()
			{
				Genode::env()->ram_session()->free(ds_cap);
			}
		};

		bool const                       _expand;
		Genode::List<Ram_chunk>          _ram_chunks;
		Timer::Connection                _timer;
		Genode::Signal_receiver          _sig_rec;
		Genode::Signal_dispatcher<Child> _periodic_timeout_dispatcher;
		Genode::Signal_dispatcher<Child> _yield_dispatcher;
		unsigned long              const _period_ms;

		void _dispatch_periodic_timeout(unsigned);
		void _dispatch_yield(unsigned);

		void _schedule_next_timeout()
		{
			_timer.trigger_once(_period_ms*1000);
		}

	public:

		Child();
		void main();
};


void Child::_dispatch_periodic_timeout(unsigned)
{
	size_t const chunk_size = 1024*1024;

	if (Genode::env()->ram_session()->avail() < chunk_size) {

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
	_ram_chunks.insert(new (Genode::env()->heap()) Ram_chunk(chunk_size));

	Genode::log("allocated chunk of ", chunk_size / 1024, " KiB");

	_schedule_next_timeout();
}


void Child::_dispatch_yield(unsigned)
{
	using namespace Genode;

	/* request yield request arguments */
	Parent::Resource_args const args = env()->parent()->yield_request();

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
		destroy(env()->heap(), chunk);
		released_quota += chunk_size;

		log("released chunk of ", chunk_size, " bytes");
	}

	/* acknowledge yield request */
	env()->parent()->yield_response();

	_schedule_next_timeout();
}


static inline unsigned long read_period_ms_from_config()
{
	unsigned long period_ms = 500;
	if (Genode::config()->xml_node().has_attribute("period_ms"))
		Genode::config()->xml_node().attribute("period_ms").value(&period_ms);
	return period_ms;
}


Child::Child()
:
	_expand(Genode::config()->xml_node().attribute_value("expand", false)),
	_periodic_timeout_dispatcher(_sig_rec, *this,
	                             &Child::_dispatch_periodic_timeout),
	_yield_dispatcher(_sig_rec, *this,
	                  &Child::_dispatch_yield),
	_period_ms(read_period_ms_from_config())
{
	/* register yield signal handler */
	Genode::env()->parent()->yield_sigh(_yield_dispatcher);

	/* register timeout signal handler and schedule periodic timeouts */
	_timer.sigh(_periodic_timeout_dispatcher);

	_schedule_next_timeout();
}


void Child::main()
{
	using namespace Genode;

	for (;;) {
		Signal sig = _sig_rec.wait_for_signal();

		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}


/*****************
 ** Parent role **
 *****************/

/**
 * The parent grants resource requests as long as it has free resources.
 * Once in a while, it politely requests the child to yield resources.
 */
class Parent : Genode::Slave_policy
{
	private:

		/**
		 * Return singleton entrypoint instance
		 *
		 * The entrypoint cannot be a regular member because we need to pass
		 * it into the constructor of the 'Slave_policy' base class.
		 */
		static Genode::Rpc_entrypoint &_entrypoint();

		typedef Genode::size_t size_t;

		size_t const slave_quota = 10*1024*1024;

		Genode::Slave _slave = { _entrypoint(), *this, slave_quota };

		Timer::Connection _timer;

		Genode::Lock _yield_blockade;

		void _print_status()
		{
			Genode::log("quota: ", _slave.ram().quota() / 1024, " KiB  "
			            "used: ",  _slave.ram().used()  / 1024, " KiB");
		}

	public:

		/**
		 * Constructor
		 */
		Parent()
		:
			Genode::Slave_policy("test-resource_yield", _entrypoint(),
			                     Genode::env()->ram_session())
		{
			configure("<config child=\"yes\" />");
		}

		int main();

		/****************************
		 ** Slave_policy interface **
		 ****************************/

		char const **_permitted_services() const
		{
			static char const *services[] = { "RM", "LOG", "Timer" };
			return services;
		}

		void yield_response()
		{
			_yield_blockade.unlock();

			/*
			 * At this point, the ownership of '_yield_blockade' will be passed
			 * to the main program. By trying to aquire it here, we block until
			 * the main program is ready.
			 *
			 * This way, we ensure that the main program validates the
			 * statistics before the 'yield_response' RPC call returns.
			 * Otherwise, the child might allocate new resources before the
			 * main program is able to see the amount of yielded resources.
			 */
			Genode::Lock::Guard guard(_yield_blockade);
		}

};


Genode::Rpc_entrypoint &Parent::_entrypoint()
{
	using namespace Genode;
	static Cap_connection cap;
	size_t const stack_size = sizeof(addr_t)*2*1024;
	static Rpc_entrypoint ep(&cap, stack_size, "ep", false);
	return ep;
}


int Parent::main()
{
	using namespace Genode;

	_entrypoint().activate();

	/* perform the test three times */
	for (unsigned j = 0; j < 3; j++) {

		/* wait five seconds and observe growth of resource usage */
		for (unsigned i = 0; i < 5; i++) {
			_timer.msleep(1000);
			_print_status();
		}

		/* remember quantum of resources used by the child */
		size_t const used_prior_yield = _slave.ram().used();

		/* issue yield request */
		Genode::Parent::Resource_args yield_args("ram_quota=5M");
		_slave.yield(yield_args);

		/*
		 * Synchronously wait for a yield response. Note that a careful parent
		 * would never trust its child to comply to the yield request.
		 */
		log("wait for yield response");
		_yield_blockade.lock();
		_yield_blockade.lock();
		log("got yield response");

		_print_status();

		/* validate that the amount of yielded resources matches the request */
		size_t const used_after_yield = _slave.ram().used();
		if (used_after_yield + 5*1024*1024 > used_prior_yield) {
			error("child has not yielded enough resources");
			return -1;
		}

		/* let the 'yield_response' RPC call return */
		_yield_blockade.unlock();
	}

	log("--- test-resource_yield finished ---");

	return 0;
}


/******************
 ** Main program **
 ******************/

int main(int argc, char **argv)
{
	using namespace Genode;

	/*
	 * Read value '<config child="" />' attribute to decide whether to perform
	 * the child or the parent role.
	 */
	bool const is_child = config()->xml_node().has_attribute("child")
	                   && config()->xml_node().attribute_value("child", false);

	if (is_child) {
		log("--- test-resource_yield child role started ---");
		static ::Child child;
		child.main();
		return -1; /* the child should never reach this point */
	} else {
		log("--- test-resource_yield parent role started ---");
		static ::Parent parent;
		return parent.main();
	}
}
