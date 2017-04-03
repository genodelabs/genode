/*
 * \brief  Test for signalling framework
 * \author Norman Feske
 * \author Martin Stein
 * \date   2008-09-06
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/thread.h>
#include <base/registry.h>
#include <timer_session/connection.h>

using namespace Genode;

/**
 * A thread that submits a signal context in a periodic fashion
 */
class Sender : Thread
{
	private:

		Timer::Connection  _timer;
		Signal_transmitter _transmitter;
		unsigned const     _interval_ms;
		bool     const     _verbose;
		bool               _stop       { false };
		unsigned           _submit_cnt { 0 };
		bool               _idle       { false };

		void entry()
		{
			while (!_stop) {
				if (!_idle) {
					_submit_cnt++;
					if (_verbose) {
						log("submit signal ", _submit_cnt); }

					_transmitter.submit();
					if (_interval_ms) {
						_timer.msleep(_interval_ms); }
				} else {
					_timer.msleep(100); }
			}
		}

	public:

		Sender(Env                       &env,
		       Signal_context_capability  context,
		       unsigned                   interval_ms,
		       bool                       verbose)
		:
			Thread(env, "sender", 4096 * sizeof(addr_t)), _timer(env),
			_transmitter(context), _interval_ms(interval_ms), _verbose(verbose)
		{
			Thread::start();
		}

		/***************
		 ** Accessors **
		 ***************/

		void     idle(bool idle)    { _idle = idle; }
		unsigned submit_cnt() const { return _submit_cnt; }
};

/**
 * A thread that receives signals and takes some time to handle each
 */
class Handler : Thread
{
	private:

		Timer::Connection  _timer;
		unsigned const     _dispatch_ms;
		unsigned const     _id;
		bool     const     _verbose;
		Signal_receiver   &_receiver;
		bool               _stop           { false };
		unsigned           _receive_cnt    { 0 };
		unsigned           _activation_cnt { 0 };
		bool               _idle           { false };

		void entry()
		{
			while (!_stop) {
				if (!_idle) {
					Signal signal = _receiver.wait_for_signal();
					if (_verbose)
						log("handler ", _id, " got ", signal.num(), " "
						    "signal", (signal.num() == 1 ? "" : "s"), " "
						    "with context ", signal.context());

					_receive_cnt += signal.num();
					_activation_cnt++;
				}
				if (_dispatch_ms)
					_timer.msleep(_dispatch_ms);
			}
		}

	public:

		Handler(Env             &env,
		        Signal_receiver &receiver,
		        unsigned         dispatch_ms,
		        bool             verbose,
		        unsigned         id)
		:
			Thread(env, "handler", 4096 * sizeof(addr_t)), _timer(env),
			_dispatch_ms(dispatch_ms), _id(id), _verbose(verbose),
			_receiver(receiver)
		{
			Thread::start();
		}

		void print(Output &output) const { Genode::print(output, "handler ", _id); }

		/***************
		 ** Accessors **
		 ***************/

		void     idle(bool idle)        { _idle = idle; }
		unsigned receive_cnt()    const { return _receive_cnt; }
		unsigned activation_cnt() const { return _activation_cnt; }
};

/**
 * Base of all signalling tests
 */
struct Signal_test
{
	enum { SPEED = 10 };

	int id;

	Signal_test(int id, char const *brief) : id(id) {
		log("\nTEST ", id, ": ", brief, "\n"); }

	~Signal_test() { log("\nTEST ", id, " finished\n"); }
};

struct Fast_sender_test : Signal_test
{
	static constexpr char const *brief =
		"reliable delivery if the sender is faster than the handlers";

	enum { HANDLER_INTERVAL_MS = 10 * SPEED,
	       SENDER_INTERVAL_MS  = 2  * SPEED,
	       DURATION_MS         = 50 * SPEED,
	       FINISH_IDLE_MS      = 2  * HANDLER_INTERVAL_MS };

	struct Unequal_sent_and_received_signals : Exception { };

	Env               &env;
	Timer::Connection  timer   { env };
	Signal_context     context;
	Signal_receiver    receiver;
	Handler            handler { env, receiver, HANDLER_INTERVAL_MS, false, 1 };
	Sender             sender  { env, receiver.manage(&context),
	                             SENDER_INTERVAL_MS, false };

	Fast_sender_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		timer.msleep(DURATION_MS);

		/* stop emitting signals */
		log("deactivate sender");
		sender.idle(true);
		timer.msleep(FINISH_IDLE_MS);
		log("sender submitted a total of ", sender.submit_cnt(), " signals");
		log("handler received a total of ", handler.receive_cnt(), " signals");

		if (sender.submit_cnt() != handler.receive_cnt()) {
			throw Unequal_sent_and_received_signals(); }
	}
};

struct Multiple_handlers_test : Signal_test
{
	static constexpr char const *brief =
		"get multiple handlers at one sender activated in a fair manner";

	enum { HANDLER_INTERVAL_MS = 8  * SPEED,
	       SENDER_INTERVAL_MS  = 1  * SPEED,
	       FINISH_IDLE_MS      = 2  * HANDLER_INTERVAL_MS,
	       DURATION_MS         = 50 * SPEED,
	       NR_OF_HANDLERS      = 4 };

	struct Unequal_sent_and_received_signals : Exception { };
	struct Unequal_activation_of_handlers    : Exception { };

	Env                            &env;
	Heap                            heap   { env.ram(), env.rm() };
	Timer::Connection               timer  { env };
	Signal_context                  context;
	Signal_receiver                 receiver;
	Registry<Registered<Handler> >  handlers;
	Sender                          sender { env, receiver.manage(&context),
	                                         SENDER_INTERVAL_MS, true};

	Multiple_handlers_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		for (unsigned i = 0; i < NR_OF_HANDLERS; i++)
			new (heap) Registered<Handler>(handlers, env, receiver,
			                               HANDLER_INTERVAL_MS, true, i);
		timer.msleep(DURATION_MS);

		/* stop emitting signals */
		log("stop generating new signals");
		sender.idle(true);
		timer.msleep(FINISH_IDLE_MS);

		/* let handlers settle down */
		handlers.for_each([&] (Handler &handler) { handler.idle(true); });
		timer.msleep(FINISH_IDLE_MS);

		/* print statistics and clean up */
		unsigned total_rcv = 0, max_act = 0, min_act = ~0;;
		handlers.for_each([&] (Handler &handler) {
			unsigned const rcv = handler.receive_cnt();
			unsigned const act = handler.activation_cnt();
			log(handler, " received ", rcv, " signals, was activated ", act, " times");
			total_rcv += rcv;
			if (act > max_act) { max_act = act; }
			if (act < min_act) { min_act = act; }
			destroy(heap, &handler);
		});
		log("sender submitted a total of ", sender.submit_cnt(), " signals");
		log("handlers received a total of ", total_rcv, " signals");

		/* check if number of sent signals match the received ones */
		if (sender.submit_cnt() != total_rcv) {
			throw Unequal_sent_and_received_signals(); }

		/* check if handlers had been activated equally (tolerance of one) */
		if (max_act - min_act > 1) {
			throw Unequal_activation_of_handlers(); }
	}
};

struct Stress_test : Signal_test
{
	static constexpr char const *brief =
		"throughput when submitting/handling as fast as possible";

	enum { DURATION_SEC = 5 };

	struct Unequal_sent_and_received_signals : Exception { };

	Env               &env;
	Timer::Connection  timer   { env };
	Signal_context     context;
	Signal_receiver    receiver;
	Handler            handler { env, receiver, 0, false, 1 };
	Sender             sender  { env, receiver.manage(&context), 0, false };

	Stress_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		for (unsigned i = 1; i <= DURATION_SEC; i++) {
			log(i, "/", (unsigned)DURATION_SEC);
			timer.msleep(1000);
		}
		log("deactivate sender");
		sender.idle(true);

		while (handler.receive_cnt() < sender.submit_cnt()) {
			log("waiting for signals still in flight...");
			timer.msleep(1000);
		}
		log("");
		log("sender submitted a total of ", sender.submit_cnt(), " signals");
		log("handler received a total of ", handler.receive_cnt(), " signals");
		log("");
		log("handler received ",      handler.receive_cnt() / DURATION_SEC, " signals per second");
		log("handler was activated ", handler.activation_cnt() / DURATION_SEC, " times per second");
		log("");

		if (sender.submit_cnt() != handler.receive_cnt())
			throw Unequal_sent_and_received_signals();
	}
};

struct Lazy_receivers_test : Signal_test
{
	static constexpr char const *brief = "lazy and out-of-order signal reception";

	Signal_context     context_1,  context_2;
	Signal_receiver    receiver_1, receiver_2;
	Signal_transmitter transmitter_1 { receiver_1.manage(&context_1) };
	Signal_transmitter transmitter_2 { receiver_2.manage(&context_2) };

	Lazy_receivers_test(Env &env, int id) : Signal_test(id, brief)
	{
		log("submit and receive signals with multiple receivers in order");
		transmitter_1.submit();
		transmitter_2.submit();
		{
			Signal signal = receiver_1.wait_for_signal();
			log("returned from wait_for_signal for receiver 1");

			signal = receiver_2.wait_for_signal();
			log("returned from wait_for_signal for receiver 2");
		}
		log("submit and receive signals with multiple receivers out of order");
		transmitter_1.submit();
		transmitter_2.submit();
		{
			Signal signal = receiver_2.wait_for_signal();
			log("returned from wait_for_signal for receiver 2");

			signal = receiver_1.wait_for_signal();
			log("returned from wait_for_signal for receiver 1");
		}
	}
};

struct Context_management_test : Signal_test
{
	static constexpr char const *brief =
		"correct initialization and cleanup of receiver and context";

	Env                       &env;
	Timer::Connection          timer       { env };
	Signal_context             context;
	Signal_receiver            receiver;
	Signal_context_capability  context_cap { receiver.manage(&context) };
	Sender                     sender      { env, context_cap, 500, true };

	Context_management_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		/* stop sender after timeout */
		timer.msleep(1000);
		log("suspend sender");
		sender.idle(true);

		/* collect pending signals and dissolve context from receiver */
		{
			Signal signal = receiver.wait_for_signal();
			log("got ", signal.num(), " signal(s) from ", signal.context());
		}
		receiver.dissolve(&context);

		/* let sender spin for some time */
		log("resume sender");
		sender.idle(false);
		timer.msleep(1000);
		log("suspend sender");
		sender.idle(true);
		log("destroy sender");
	}
};

struct Synchronized_destruction_test : Signal_test, Thread
{
	static constexpr char const *brief =
		"does 'dissolve' block as long as the signal context is referenced?";

	struct Failed : Exception { };

	Env                &env;
	Timer::Connection   timer        { env };
	Heap                heap         { env.ram(), env.rm() };
	Signal_context     &context      { *new (heap) Signal_context };
	Signal_receiver     receiver;
	Signal_transmitter  transmitter  { receiver.manage(&context) };
	bool                destroyed    { false };

	void entry()
	{
		receiver.dissolve(&context);
		log("dissolve finished");
		destroyed = true;
		destroy(heap, &context);
	}

	Synchronized_destruction_test(Env &env, int id)
	: Signal_test(id, brief), Thread(env, "destroyer", 1024 * sizeof(addr_t)), env(env)
	{
		transmitter.submit();
		{
			Signal signal = receiver.wait_for_signal();
			log("start dissolving");
			Thread::start();
			timer.msleep(2000);
			Signal signal_copy_1 = signal;
			Signal signal_copy_2 = signal;
			signal_copy_1        = signal_copy_2;
			if (destroyed) {
				throw Failed(); }
			log("destruct signal");
		}
		Thread::join();
	}
};

struct Many_contexts_test : Signal_test
{
	static constexpr char const *brief = "create and manage many contexts";

	struct Manage_failed : Exception { };

	Env                                   &env;
	Heap                                   heap { env.ram(), env.rm() };
	Registry<Registered<Signal_context> >  contexts;

	Many_contexts_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		for (unsigned round = 0; round < 10; round++) {

			unsigned const nr_of_contexts = 200 + 5 * round;
			log("round ", round, ": manage ", nr_of_contexts, " contexts");

			Signal_receiver receiver;
			for (unsigned i = 0; i < nr_of_contexts; i++) {
				if (!receiver.manage(new (heap) Registered<Signal_context>(contexts)).valid()) {
					throw Manage_failed(); }
			}
			contexts.for_each([&] (Registered<Signal_context> &context) {
				receiver.dissolve(&context);
				destroy(heap, &context);
			});
		}
	}
};

/**
 * Test 'wait_and_dispatch_one_io_signal' implementation for entrypoints
 *
 * Normally Genode signals are delivered by a signal thread, which blocks for
 * incoming signals and is woken up when a signals arrives, the thread then
 * sends an RPC to an entrypoint that, in turn, processes the signal.
 * 'wait_and_dispatch_one_io_signal' allows an entrypoint to receive I/O-level
 * signals directly, by taking advantage of the same code as the signal thread.
 * This leaves the problem that at this point two entities (the signal thread
 * and the entrypoint) may wait for signals to arrive. It is not decidable
 * which entity is woken up on signal arrival. If the signal thread is woken up
 * and tries to deliver the signal RPC, system may dead lock when no additional
 * signal arrives to pull the entrypoint out of the signal waiting code. This
 * test triggers this exact situation. We also test nesting with the same
 * signal context of 'wait_and_dispatch_one_io_signal' here, which also caused
 * dead locks in the past. Also, the test verifies application-level signals
 * are deferred during 'wait_and_dispatch_one_io_signal'.
 */
struct Nested_test : Signal_test
{
	static constexpr char const *brief = "wait and dispatch signals at entrypoint";

	struct Test_interface
	{
		GENODE_RPC(Rpc_test_io_dispatch,  void, test_io_dispatch);
		GENODE_RPC(Rpc_test_app_dispatch, void, test_app_dispatch);
		GENODE_RPC_INTERFACE(Rpc_test_io_dispatch, Rpc_test_app_dispatch);
	};

	struct Test_component : Rpc_object<Test_interface, Test_component>
	{
		Nested_test &test;

		Test_component(Nested_test &test) : test(test) { }

		void test_io_dispatch()
		{
			log("1/8: [ep] wait for I/O-level signal during RPC from [outside]");
			while (!test.io_done) test.ep.wait_and_dispatch_one_io_signal();
			log("6/8: [ep] I/O completed");
		}

		void test_app_dispatch()
		{
			if (!test.app_done)
				error("8/8: [ep] application-level signal was not dispatched");
			else
				log("8/8: [ep] success");
		}
	};

	struct Sender_thread : Thread
	{
		Nested_test       &test;
		Timer::Connection  timer;

		Sender_thread(Env &env, Nested_test &test)
		:
			Thread(env, "sender_thread", 1024 * sizeof(long)),
			test(test), timer(env)
		{ }

		void entry()
		{
			timer.msleep(1000);

			log("2/8: [outside] submit application-level signal (should be deferred)");
			Signal_transmitter(test.nop_handler).submit();
			Signal_transmitter(test.app_handler).submit();
			Signal_transmitter(test.nop_handler).submit();

			log("3/8: [outside] submit I/O-level signal");
			Signal_transmitter(test.io_handler).submit();
			Signal_transmitter(test.nop_handler).submit();
		}
	};

	Env &env;
	Entrypoint ep { env, 2048 * sizeof(long), "wait_dispatch_ep" };

	Signal_handler<Nested_test>   app_handler { ep, *this, &Nested_test::handle_app };
	Signal_handler<Nested_test>   nop_handler { ep, *this, &Nested_test::handle_nop };
	Io_signal_handler<Nested_test> io_handler { ep, *this, &Nested_test::handle_io };

	Test_component             wait     { *this };
	Capability<Test_interface> wait_cap { ep.manage(wait) };
	Sender_thread              thread   { env, *this };
	bool                       nested   { false };
	bool volatile              app_done { false };
	bool volatile              io_done  { false };

	Timer::Connection timer { env };

	Nested_test(Env &env, int id) : Signal_test(id, brief), env(env)
	{
		thread.start();
		wait_cap.call<Test_interface::Rpc_test_io_dispatch>();

		/* grant the ep some time for application-signal handling */
		timer.msleep(1000);
		wait_cap.call<Test_interface::Rpc_test_app_dispatch>();
	}

	~Nested_test()
	{
		ep.dissolve(wait);
	}

	void handle_app()
	{
		if (!io_done)
			error("7/8: [ep] application-level signal was not deferred");
		else
			log("7/8: [ep] application-level signal received");

		app_done = true;
	}

	void handle_nop() { }

	void handle_io()
	{
		if (nested) {
			log("5/8: [ep] nested I/O-level signal received");
			io_done = true;
			return;
		}

		log("4/8: [ep] I/O-level signal received - sending nested signal");
		nested = true;
		Signal_transmitter(io_handler).submit();
		ep.wait_and_dispatch_one_io_signal();
	}
};

/**
 * Stress-test 'wait_and_dispatch_one_io_signal' implementation for entrypoints
 *
 * Let multiple entrypoints directly wait and dispatch signals in a
 * highly nested manner and with multiple stressful senders.
 */
struct Nested_stress_test : Signal_test
{
	static constexpr char const *brief = "stressful wait and dispatch signals at entrypoint";

	enum {
		COUNTER_GOAL          = 300,
		UNWIND_COUNT_MOD_LOG2 = 5,
		POLLING_PERIOD_US     = 1000000,
	};

	struct Sender : Thread
	{
		Signal_transmitter transmitter;
		bool               destruct { false };

		Sender(Env &env, char const *name, Signal_context_capability cap)
		: Thread(env, name, 1024 * sizeof(long)), transmitter(cap) { }

		void entry()
		{
			/* send signals as fast as possible */
			while (!destruct) { transmitter.submit(); }
		}
	};

	struct Receiver
	{
		Entrypoint  ep;
		char const *name;
		unsigned    count    { 0 };
		bool        destruct { false };

		Io_signal_handler<Receiver> handler { ep, *this, &Receiver::handle };

		Receiver(Env &env, char const *name)
		: ep(env, 3 * 1024 * sizeof(long), name), name(name) { }

		void handle()
		{
			/*
			 * We have to get out of the nesting if the host wants to destroy
			 * us to avoid a deadlock at the lock in the signal handler.
			 */
			if (destruct) { return; }

			/* raise call counter */
			count++;

			/*
			 * Open a new nesting level with each signal until count module X
			 * gives zero, then unwind the whole nesting and start afresh.
			 */
			if ((count & ((1 << UNWIND_COUNT_MOD_LOG2) - 1)) != 0) {
				ep.wait_and_dispatch_one_io_signal(); }
		}
	};

	Env                &env;
	Timer::Connection   timer      { env };
	Receiver            receiver_1 { env, "receiver-1" };
	Receiver            receiver_2 { env, "receiver-2" };
	Receiver            receiver_3 { env, "receiver-3" };
	Sender              sender_1   { env, "sender-1", receiver_1.handler };
	Sender              sender_2   { env, "sender-2", receiver_2.handler };
	Sender              sender_3   { env, "sender-3", receiver_3.handler };
	Signal_transmitter  done;

	Io_signal_handler<Nested_stress_test> poll {
		env.ep(), *this, &Nested_stress_test::handle_poll };

	Nested_stress_test(Env &env, int id, Signal_context_capability done)
	: Signal_test(id, brief), env(env), done(done)
	{
		/* let senders start sending signals like crazy */
		sender_1.start();
		sender_2.start();
		sender_3.start();

		/* initialize polling for the receiver counts */
		timer.sigh(poll);
		timer.trigger_periodic(POLLING_PERIOD_US);
	}

	~Nested_stress_test()
	{
		/* tell timer not to send any signals anymore. */
		timer.sigh(Timer::Session::Signal_context_capability());

		/* let senders stop burning our CPU time */
		sender_1.destruct = true;
		sender_2.destruct = true;
		sender_3.destruct = true;

		/* let receivers unwind their nesting and stop with the next signal */
		receiver_1.destruct = true;
		receiver_2.destruct = true;
		receiver_3.destruct = true;

		/*
		 * Send final signals ourselves because otherwise we would have to
		 * synchronize with the senders.
		 */
		sender_1.transmitter.submit();
		sender_2.transmitter.submit();
		sender_3.transmitter.submit();

		/* wait until threads joined */
		sender_1.join(); sender_2.join(), sender_3.join();
	}

	void handle_poll()
	{
		/* print counter status */
		log(receiver_1.name, " received ", receiver_1.count, " times");
		log(receiver_2.name, " received ", receiver_2.count, " times");
		log(receiver_3.name, " received ", receiver_3.count, " times");

		/* request to end the test if receiver counts are all high enough */
		if (receiver_1.count > COUNTER_GOAL &&
		    receiver_2.count > COUNTER_GOAL &&
		    receiver_3.count > COUNTER_GOAL)
		{ done.submit(); }
	}
};

struct Main
{
	Env                  &env;
	Signal_handler<Main>  test_9_done { env.ep(), *this, &Main::handle_test_9_done };

	Constructible<Fast_sender_test>              test_1;
	Constructible<Multiple_handlers_test>        test_2;
	Constructible<Stress_test>                   test_3;
	Constructible<Lazy_receivers_test>           test_4;
	Constructible<Context_management_test>       test_5;
	Constructible<Synchronized_destruction_test> test_6;
	Constructible<Many_contexts_test>            test_7;
	Constructible<Nested_test>                   test_8;
	Constructible<Nested_stress_test>            test_9;

	void handle_test_9_done()
	{
		test_9.destruct();
		log("--- Signalling test finished ---");
	}

	Main(Env &env) : env(env)
	{
		log("--- Signalling test ---");
		test_1.construct(env, 1); test_1.destruct();
		test_2.construct(env, 2); test_2.destruct();
		test_3.construct(env, 3); test_3.destruct();
		test_4.construct(env, 4); test_4.destruct();
		test_5.construct(env, 5); test_5.destruct();
		test_6.construct(env, 6); test_6.destruct();
		test_7.construct(env, 7); test_7.destruct();
		test_8.construct(env, 8); test_8.destruct();
		test_9.construct(env, 9, test_9_done);
	}
};

void Component::construct(Genode::Env &env) { static Main main(env); }
