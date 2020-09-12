/*
 * \brief  Multiprocessor testsuite
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2013-07-19
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <trace/timestamp.h>


namespace Genode {

	static inline void print(Output &out, Affinity::Location location)
	{
		print(out, location.xpos(), ",", location.ypos());
	}
}

/**
 * Set up a server running on every CPU one Rpc_entrypoint
 */
namespace Mp_server_test {

	/**
	 * Test session interface definition
	 */
	struct Session : Genode::Session
	{
		static const char *service_name() { return "MP_RPC_TEST"; }

		enum { CAP_QUOTA = 2 };

		GENODE_RPC(Rpc_test_untyped, void, test_untyped, unsigned);
		GENODE_RPC(Rpc_test_cap, void, test_cap, Genode::Native_capability);
		GENODE_RPC(Rpc_test_cap_reply, Genode::Native_capability,
		           test_cap_reply, Genode::Native_capability);
		GENODE_RPC_INTERFACE(Rpc_test_untyped, Rpc_test_cap, Rpc_test_cap_reply);
	};

	struct Client : Genode::Rpc_client<Session>
	{
		Client(Genode::Capability<Session> cap) : Rpc_client<Session>(cap) { }

		void test_untyped(unsigned value) { call<Rpc_test_untyped>(value); }
		void test_cap(Genode::Native_capability cap) { call<Rpc_test_cap>(cap); }
		Genode::Native_capability test_cap_reply(Genode::Native_capability cap) {
			return call<Rpc_test_cap_reply>(cap); }
	};

	struct Component : Genode::Rpc_object<Session, Component>
	{
		/* Test to just sent plain words (untyped items) */
		void test_untyped(unsigned);
		/* Test to transfer a object capability during send */
		void test_cap(Genode::Native_capability);
		/* Test to transfer a object capability during send+reply */
		Genode::Native_capability test_cap_reply(Genode::Native_capability);
	};

	typedef Genode::Capability<Session> Capability;

	struct Cpu_compound
	{
		enum { STACK_SIZE = 2*1024*sizeof(long) };

		Genode::Rpc_entrypoint rpc;
		Component              comp { };
		Capability             cap  { rpc.manage(&comp) };
		Client                 cli  { cap };

		Cpu_compound(Genode::Affinity::Location l, Genode::Env &env)
		: rpc(&env.pd(), STACK_SIZE, "rpc en", l) {}
		~Cpu_compound() { rpc.dissolve(&comp); }
	};

	/**
	 * Session implementation
	 */
	void Component::test_untyped(unsigned value) {
		Genode::log("RPC: function ", __FUNCTION__, ": got value ", value);
	}

	void Component::test_cap(Genode::Native_capability cap) {
		Genode::log("RPC: function ", __FUNCTION__, ": capability is valid ? ",
		            cap.valid() ? "yes" : "no", " - idx ", cap.local_name());
	}

	Genode::Native_capability Component::test_cap_reply(Genode::Native_capability cap) {
		Genode::log("RPC: function ", __FUNCTION__, ": capability is valid ? ",
		            cap.valid() ? "yes" : "no", " - idx ", cap.local_name());
		return cap;
	}
	
	static void execute(Genode::Env & env, Genode::Heap & heap,
	                    Genode::Affinity::Space & cpus)
	{
		using namespace Genode;

		log("RPC: --- test started ---");

		Cpu_compound ** compounds = new (heap) Cpu_compound*[cpus.total()];
		for (unsigned i = 0; i < cpus.total(); i++)
			compounds[i] = new (heap) Cpu_compound(cpus.location_of_index(i), env);

		/* Invoke RPC entrypoint on different CPUs */
		for (unsigned i = 0; i < cpus.total(); i++) {
			log("RPC: call server on CPU ", i);
			compounds[i]->cli.test_untyped(i);
		}

		/* Transfer a capability to RPC Entrypoints on different CPUs */
		for (unsigned i = 0; i < cpus.total(); i++) {
			Native_capability cap = compounds[0]->cap;
			log("RPC: call server on CPU ", i, " - transfer cap ", cap.local_name());
			compounds[i]->cli.test_cap(cap);
		}

		/* Transfer a capability to RPC Entrypoints and back */
		for (unsigned i = 0; i < cpus.total(); i++) {
			Native_capability cap  = compounds[0]->cap;
			log("RPC: call server on CPU ", i, " - transfer cap ", cap.local_name());
			Native_capability rcap = compounds[i]->cli.test_cap_reply(cap);
			log("RPC: got from server on CPU ", i, " - received cap ", rcap.local_name());
		}

		/* clean up */
		for (unsigned i = 0; i < cpus.total(); i++)
			destroy(heap, compounds[i]);
		destroy(heap, compounds);

		log("RPC: --- test finished ---");
	}
}

namespace Affinity_test {

	enum {
		STACK_SIZE = sizeof(long)*2048,
		COUNT_VALUE = 10 * 1024 * 1024
	};

	struct Spinning_thread : Genode::Thread
	{
		Genode::Affinity::Location const location;
		Genode::uint64_t volatile        cnt;
		Genode::Blockade                 barrier { };

		void entry() override
		{
			barrier.wakeup();
			Genode::log("Affinity: thread started on CPU ",
			            location, " spinning...");

			for (;;) cnt++;
		}

		Spinning_thread(Genode::Env &env, Location location)
		: Genode::Thread(env, Name("spinning_thread"), STACK_SIZE, location,
		                 Weight(), env.cpu()),
		  location(location), cnt(0ULL) {
			start(); }
	};

	void execute(Genode::Env &env, Genode::Heap & heap,
	             Genode::Affinity::Space & cpus)
	{
		using namespace Genode;

		log("Affinity: --- test started ---");

		/* get some memory for the thread objects */
		Spinning_thread ** threads = new (heap) Spinning_thread*[cpus.total()];
		uint64_t * thread_cnt = new (heap) uint64_t[cpus.total()];

		/* construct the thread objects */
		for (unsigned i = 0; i < cpus.total(); i++)
			threads[i] = new (heap)
				Spinning_thread(env, cpus.location_of_index(i));

		/* wait until all threads are up and running */
		for (unsigned i = 0; i < cpus.total(); i++)
			threads[i]->barrier.block();

		log("Affinity: Threads started on a different CPU each.");
		log("Affinity: You may inspect them using the kernel debugger - if you have one.");
		log("Affinity: Main thread monitors client threads and prints the status of them.");
		log("Affinity: Legend : D - DEAD, A - ALIVE");

		volatile uint64_t cnt = 0;
		unsigned round = 0;

		char const   text_cpu[] = "Affinity:      CPU: ";
		char const text_round[] = "Affinity: Round %2u: ";
		char * output_buffer = new (heap) char [sizeof(text_cpu) + 3 * cpus.total()];

		for (; round < 11;) {
			cnt++;

			/* try to get a life sign by the main thread from the remote threads */
			if (cnt % COUNT_VALUE == 0) {
				char * output = output_buffer;
				snprintf(output, sizeof(text_cpu), text_cpu);
				output += sizeof(text_cpu) - 1;
				for (unsigned i = 0; i < cpus.total(); i++) {
					snprintf(output, 4, "%2u ", i);
					output += 3;
				}
				log(Cstring(output_buffer));

				output = output_buffer;
				snprintf(output, sizeof(text_round), text_round, round);
				output += sizeof(text_round) - 2;

				for (unsigned i = 0; i < cpus.total(); i++) {
					snprintf(output, 4, "%s ",
							 thread_cnt[i] == threads[i]->cnt ? " D" : " A");
					output += 3;
					thread_cnt[i] = threads[i]->cnt;
				}
				log(Cstring(output_buffer));

				round ++;
			}
		}

		destroy(heap, output_buffer);

		for (unsigned i = 0; i < cpus.total(); i++)
			destroy(heap, threads[i]);
		destroy(heap, threads);
		destroy(heap, thread_cnt);
		log("Affinity: --- test finished ---");
	}
}

namespace Tlb_shootdown_test {

	struct Thread : Genode::Thread, Genode::Noncopyable
	{
		enum { STACK_SIZE = sizeof(long)*2048 };

		unsigned            cpu_idx;
		volatile unsigned * values;
		Genode::Blockade    barrier { };

		void entry() override
		{
			Genode::log("TLB: thread started on CPU ", cpu_idx);
			values[cpu_idx] = 1;
			barrier.wakeup();

			for (; values[cpu_idx] == 1;) ;

			Genode::raw("Unforseeable crosstalk effect!");
		}

		Thread(Genode::Env &env, Location location, unsigned idx,
		       volatile unsigned * values)
		: Genode::Thread(env, Name("tlb_thread"), STACK_SIZE, location,
		                 Weight(), env.cpu()),
		  cpu_idx(idx), values(values) {
			  start(); }

		/*
		 * Noncopyable
		 */
		Thread(Thread const&);
		Thread &operator = (Thread const &);
	};

	void execute(Genode::Env &env, Genode::Heap & heap,
	             Genode::Affinity::Space & cpus)
	{
		using namespace Genode;

		log("TLB: --- test started ---");

		enum { DS_SIZE = 4096 };
		Genode::Attached_ram_dataspace * ram_ds =
			new (heap) Genode::Attached_ram_dataspace(env.ram(), env.rm(),
			                                          DS_SIZE);

		/* get some memory for the thread objects */
		Thread ** threads = new (heap) Thread*[cpus.total()];

		/* construct the thread objects */
		for (unsigned i = 1; i < cpus.total(); i++)
			threads[i] = new (heap) Thread(env, cpus.location_of_index(i), i,
			                               ram_ds->local_addr<volatile unsigned>());

		/* wait until all threads are up and running */
		for (unsigned i = 1; i < cpus.total(); i++) threads[i]->barrier.block();

		log("TLB: all threads are up and running...");
		destroy(heap, ram_ds);
		log("TLB: ram dataspace destroyed, all have to fail...");

		/**
		 * The more cores are existing the more threads have to fail.
		 * The bottleneck is core's page-fault messages all printed
		 * over a lazy serial line from core 0.
		 * We have to wait here, for some time so that all fault
		 * messages are received before the test finishes.
		 */
		for (volatile unsigned i = 0; i < (0x2000000 * cpus.total()); i++) ;

		for (unsigned i = 1; i < cpus.total(); i++) destroy(heap, threads[i]);
		destroy(heap, threads);

		log("TLB: --- test finished ---");
	}
}

namespace Tsc_test {

	struct Tsc_thread : Genode::Thread
	{
		enum { STACK_SIZE = 4 * 4096 };

		Genode::Affinity::Location  const    location;
		Genode::Blockade                     barrier   { };
		Genode::uint64_t            volatile cnt       { 0 };
		Genode::uint64_t            volatile tsc_value { 0 };
		Genode::uint64_t                     last_cnt  { 0 };
		Genode::uint64_t                     last_tsc  { 0 };
		Genode::uint64_t                     diff      { 0 };
		bool                        volatile loop      { true };
		bool                        volatile spin      { true };

		void entry() override
		{
			last_tsc = Genode::Trace::timestamp();

			Genode::log(this, " ", Genode::Hex(last_tsc));
			barrier.wakeup();

			while (loop) {
				while (spin && loop) cnt++;

				measure();
				spin = true;
			}
		}

		void measure() { tsc_value = Genode::Trace::timestamp(); }

		Tsc_thread(Genode::Env &env, Location location)
		: Genode::Thread(env, Name("tsc_thread"), STACK_SIZE, location,
		                 Weight(), env.cpu()), location(location)
		{ }
	};

	template <int T>
	Genode::String<T> _align_right(Genode::String<T> const &s)
	{
		Genode::String<T> result = s;

		for (Genode::uint64_t i = s.length(); i < T; i++)
			result = Genode::String<T>(" ", result);

		return result;
	}

	template <int T>
	Genode::String<T> _align_right(Genode::uint64_t const value)
	{
		Genode::String<T> result("",Genode::Hex(value));

		Genode::uint64_t pow = 16;

		for (Genode::uint64_t i = 3; i < (T - 1); i++, pow *= 16) {
			if (value < pow) {
				result = Genode::String<T>(" ", result);
			}
		}

		if (value > pow) {
			result = Genode::String<T>("?");
			for (Genode::uint64_t i = 1; i < (T - 1); i++)
				result = Genode::String<T>(" ", result);
		}

		return result;
	}

	void execute(Genode::Env &env, Genode::Heap &heap,
	             Genode::Affinity::Space &cpus)
	{
		using namespace Genode;

		log("TSC: --- test started ---");

		/* get some memory for the thread objects */
		Tsc_thread ** threads = new (heap) Tsc_thread*[cpus.total()];

		/* construct the thread objects */
		for (unsigned i = 0; i < cpus.total(); i++) {
			threads[i] = new (heap) Tsc_thread(env, cpus.location_of_index(i));
			/* skip first thread, current thread will do the measurement */
			if (i) threads[i]->start();
		}

		/* wait until all threads are up and running */
		for (unsigned i = 1; i < cpus.total(); i++) threads[i]->barrier.block();

		{
			String<128> legend("   ");
			for (unsigned i = 0; i < cpus.total(); i++) {
				legend = String<128>(legend, _align_right<15>(String<128>("cpu (", threads[i]->affinity(),")")));
			}
			legend = String<128>(legend, _align_right<13>("diff-min"));
			legend = String<128>(legend, _align_right<13>("diff-max"));
			log(legend);
		}

		log("round / tsc per cpu");

		/* we handle the first cpu */
		threads[0]->measure();
		threads[0]->last_tsc = threads[0]->tsc_value;

		/* make some rounds */
		for (unsigned round = 0; round < 20; round++) {

			/* stop spinning */
			for (unsigned i = 1; i < cpus.total(); i++)
				threads[i]->spin = false;

			/* wait for valid results */
			for (unsigned i = 1; i < cpus.total(); i++)
				while (!threads[i]->spin) { };

			/* do measure for cpu 0 */
			threads[0]->measure();

			/* calculate results */
			String<128> show;
			String<128> show_diff;

			for (unsigned i = 0; i < cpus.total(); i++) {
				uint64_t diff = threads[i]->tsc_value - threads[i]->last_tsc;
				if (round) {
					bool plus = diff > threads[i]->diff;
					show_diff = String<128>(show_diff, " ", plus ? "+" : "-",
					                        _align_right<13>(plus ?
					                            (diff - threads[i]->diff) :
					                            (threads[i]->diff - diff)));
				}

				threads[i]->diff     = diff;
				threads[i]->last_cnt = threads[i]->cnt;
				threads[i]->last_tsc = threads[i]->tsc_value;

				show = String<128>(show, " ", _align_right<14>(threads[i]->diff));

			}
			uint64_t min_diff = ~0ULL;
			uint64_t max_diff = 0;

			for (unsigned i = 0; i < cpus.total(); i++) {
				for (unsigned j = 0; j < cpus.total(); j++) {
					if (i == j) continue;

					uint64_t diff = (threads[i]->diff > threads[j]->diff) ?
					                (threads[i]->diff - threads[j]->diff) :
					                (threads[j]->diff - threads[i]->diff);

					if (diff < min_diff) min_diff = diff;
					if (diff > max_diff) max_diff = diff;
				}
			}

			/* show result */
			if (round)
				log("   ", show_diff); /* diff to prev column */
			log(round, round < 10 ? "  " : " ", show,
			    " ", _align_right<12>(min_diff),
			    " ", _align_right<12>(max_diff));
		}

		/* break loop and stop spinning */
		for (unsigned i = 1; i < cpus.total(); i++) threads[i]->loop = false;

		/* join finished worker threads */
		for (unsigned i = 1; i < cpus.total(); i++) threads[i]->join();

		/* cleanup */
		for (unsigned i = 0; i < cpus.total(); i++) destroy(heap, threads[i]);
		destroy(heap, threads);

		log("TSC: --- test finished ---");
	}
}

void Component::construct(Genode::Env & env)
{
	using namespace Genode;

	log("--- SMP testsuite started ---");

	Affinity::Space cpus = env.cpu().affinity_space();
	log("Detected ", cpus.width(), "x", cpus.height(), " CPU",
	    cpus.total() > 1 ? "s." : ".");

	Heap heap(env.ram(), env.rm());

	Tsc_test::execute(env, heap, cpus);
	Mp_server_test::execute(env, heap, cpus);
	Affinity_test::execute(env, heap, cpus);
	Tlb_shootdown_test::execute(env, heap, cpus);

	log("--- SMP testsuite finished ---");
}
