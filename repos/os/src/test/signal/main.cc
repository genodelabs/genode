/*
 * \brief  Test for signalling framework
 * \author Norman Feske
 * \date   2008-09-06
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/signal.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>
#include <util/misc_math.h>

using namespace Genode;


/**
 * Transmit signals in a periodic fashion
 */
class Sender : Thread<4096>
{
	private:

		Signal_transmitter _transmitter;
		Timer::Connection  _timer;        /* timer connection for local use           */
		unsigned           _interval_ms;  /* interval between signals in milliseconds */
		bool               _stop;         /* state for destruction protocol           */
		unsigned           _submit_cnt;   /* statistics                               */
		bool               _idle;         /* suppress the submission of signals       */
		bool               _verbose;      /* print activities                         */

		/**
		 * Sender thread submits signals every '_interval_ms' milliseconds
		 */
		void entry()
		{
			while (!_stop) {

				if (!_idle) {
					_submit_cnt++;

					if (_verbose)
						printf("submit signal %d\n", _submit_cnt);

					_transmitter.submit();

					if (_interval_ms)
						_timer.msleep(_interval_ms);
				} else
					_timer.msleep(100);
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param context      signal destination
		 * \param interval_ms  interval between signals
		 * \param verbose      print status information
		 */
		Sender(Signal_context_capability context,
		       unsigned interval_ms, bool verbose = true)
		:
			Thread("sender"),
			_transmitter(context),
			_interval_ms(interval_ms),
			_stop(false),
			_submit_cnt(0),
			_idle(0),
			_verbose(verbose)
		{
			/* start thread at 'entry' function */
			start();
		}

		/**
		 * Destructor
		 */
		~Sender()
		{
			/* tell thread to stop iterating */
			_stop = true;

			/* wait for current 'msleep' call of the thread to finish */
			_timer.msleep(0);
		}

		/**
		 * Suppress the transmission of further signals
		 */
		void idle(bool idle = true) { _idle = idle; }

		/**
		 * Return total number of submitted notifications
		 */
		unsigned submit_cnt() { return _submit_cnt; }
};


/**
 * Signal handler receives signals and takes some time to handle each
 */
class Handler : Thread<4096>
{
	private:

		unsigned         _dispatch_ms;      /* time needed for dispatching a signal         */
		unsigned         _id;               /* unique ID of signal handler for debug output */
		static unsigned  _id_cnt;           /* counter for producing unique IDs             */
		Signal_receiver *_receiver;         /* signal endpoint                              */
		Timer::Connection _timer;           /* timer connection for local use               */
		bool              _stop;            /* state for destruction protocol               */
		unsigned          _receive_cnt;     /* number of received notifications             */
		unsigned          _activation_cnt;  /* number of invocations of the signal handler  */
		bool              _idle;            /* suppress the further handling of signals     */
		bool              _verbose;         /* print status information                     */

		/**
		 * Signal handler needs '_dispatch_ms' milliseconds for each signal
		 */
		void entry()
		{
			while (!_stop) {

				if (!_idle) {
					Signal signal = _receiver->wait_for_signal();

					if (_verbose)
						printf("handler %d got %u signal%s with context %p\n",
						       _id,
						       signal.num(),
						       signal.num() == 1 ? "" : "s",
						       signal.context());

					_receive_cnt += signal.num();
					_activation_cnt++;
				}

				if (_dispatch_ms)
					_timer.msleep(_dispatch_ms);
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param receiver     receiver to request signals from
		 * \param dispatch_ms  duration of signal-handler activity
		 * \param verbose      print status information
		 */
		Handler(Signal_receiver *receiver, unsigned dispatch_ms, bool verbose = true)
		:
			Thread("handler"),
			_dispatch_ms(dispatch_ms),
			_id(++_id_cnt),
			_receiver(receiver),
			_stop(false),
			_receive_cnt(0),
			_activation_cnt(0),
			_idle(0),
			_verbose(verbose)
		{
			start();
		}

		/**
		 * Destructor
		 */
		~Handler()
		{
			/* tell thread to stop iterating */
			_stop = true;

			/* wait for current 'msleep' call of the thread to finish */
			_timer.msleep(0);
		}

		/**
		 * Suppress the handling of further signals
		 */
		void idle(bool idle = true) { _idle = idle; }

		/**
		 * Return total number of received notifications
		 */
		unsigned receive_cnt() const { return _receive_cnt; }

		/**
		 * Return total number of signal-handler activations
		 */
		unsigned activation_cnt() const { return _activation_cnt; }
};


/**
 * Counter for generating unique signal-handler IDs
 */
unsigned Handler::_id_cnt = 0;


/**
 * Counter for enumerating the tests
 */
static unsigned test_cnt = 0;


/**
 * Timer connection to be used by the main program
 */
static Timer::Connection timer;


/**
 * Connection to CAP service used for allocating signal-context capabilities
 */
static Genode::Cap_connection cap;


/**
 * Symbolic error codes
 */
class Test_failed { };
class Test_failed_with_unequal_sent_and_received_signals : public Test_failed { };
class Test_failed_with_unequal_activation_of_handlers    : public Test_failed { };


class Id_signal_context : public Signal_context
{
	private:

		int _id;

	public:

		Id_signal_context(int id) : _id(id) { }

		int id() const { return _id; }
};


/**
 * Test for reliable notification delivery
 *
 * For this test, the produce more notification than that can be handled.
 * Still, the total number of notifications gets transmitted because of the
 * batching of notifications in one signal. This test fails if the number of
 * submitted notifications on the sender side does not match the number of
 * notifications received at the signal handler.
 */
static void fast_sender_test()
{
	enum { SPEED            = 10 };
	enum { TEST_DURATION    = 50*SPEED };
	enum { HANDLER_INTERVAL = 10*SPEED };
	enum { SENDER_INTERVAL  = 2*SPEED };
	enum { FINISH_IDLE_TIME = 2*HANDLER_INTERVAL };

	printf("\n");
	printf("TEST %d: one sender, one handler, sender is faster than handler\n", ++test_cnt);
	printf("\n");

	Signal_receiver receiver;
	Id_signal_context context_123(123);

	Handler *handler = new (env()->heap()) Handler(&receiver, HANDLER_INTERVAL, false);
	Sender  *sender  = new (env()->heap()) Sender(receiver.manage(&context_123),
	                                              SENDER_INTERVAL, false);

	timer.msleep(TEST_DURATION);

	/* stop emitting signals */
	printf("deactivate sender\n");
	sender->idle();
	timer.msleep(FINISH_IDLE_TIME);

	printf("\n");
	printf("sender submitted a total of %d signals\n", sender->submit_cnt());
	printf("handler received a total of %d signals\n", handler->receive_cnt());
	printf("\n");

	if (sender->submit_cnt() != handler->receive_cnt())
		throw Test_failed();

	receiver.dissolve(&context_123);

	destroy(env()->heap(), sender);
	destroy(env()->heap(), handler);

	printf("TEST %d FINISHED\n", test_cnt);
}


/**
 * Fairness test if multiple signal-handler threads are present at one receiver
 *
 * We expect that all handler threads get activated in a fair manner. The test
 * fails if the number of activations per handler differs by more than one.
 * Furthermore, if operating in non-descrete mode, the total number of sent and
 * handled notifications is checked.
 */
static void multiple_handlers_test()
{
	enum { SPEED            = 10 };
	enum { TEST_DURATION    = 50*SPEED };
	enum { HANDLER_INTERVAL = 8*SPEED };
	enum { SENDER_INTERVAL  = 1*SPEED };
	enum { FINISH_IDLE_TIME = 2*HANDLER_INTERVAL };
	enum { NUM_HANDLERS     = 4 };

	printf("\n");
	printf("TEST %d: one busy sender, %d handlers\n",
	       ++test_cnt, NUM_HANDLERS);
	printf("\n");

	Signal_receiver receiver;

	Handler *handler[NUM_HANDLERS];
	for (int i = 0; i < NUM_HANDLERS; i++)
		handler[i] = new (env()->heap()) Handler(&receiver, HANDLER_INTERVAL);

	Id_signal_context context_123(123);
	Sender *sender = new (env()->heap()) Sender(receiver.manage(&context_123), SENDER_INTERVAL);

	timer.msleep(TEST_DURATION);

	/* stop emitting signals */
	printf("stop generating new notifications\n");
	sender->idle();
	timer.msleep(FINISH_IDLE_TIME);

	/* let handlers settle down */
	for (int i = 0; i < NUM_HANDLERS; i++)
		handler[i]->idle();
	timer.msleep(FINISH_IDLE_TIME);

	/* print signal delivery statistics */
	printf("\n");
	printf("sender submitted a total of %d signals\n",
	        sender->submit_cnt());
	unsigned total_receive_cnt = 0;
	for (int i = 0; i < NUM_HANDLERS; i++) {
		printf("handler %d received a total of %d signals\n",
		       i, handler[i]->receive_cnt());
		total_receive_cnt += handler[i]->receive_cnt();
	}
	printf("all handlers received a total of %d signals\n",
	       total_receive_cnt);

	/* check if number of sent notifications match the received ones */
	if (sender->submit_cnt() != total_receive_cnt)
		throw Test_failed_with_unequal_sent_and_received_signals();

	/* print activation statistics */
	printf("\n");
	for (int i = 0; i < NUM_HANDLERS; i++)
		printf("handler %d was activated %d times\n",
		       i, handler[i]->activation_cnt());
	printf("\n");

	/* check if handlers had been activated equally (tolerating a difference of one) */
	for (int i = 0; i < NUM_HANDLERS; i++) {

		int diff = handler[0]->activation_cnt()
		         - handler[(i + 1)/NUM_HANDLERS]->activation_cnt();

		if (abs(diff) > 1)
			throw Test_failed_with_unequal_activation_of_handlers();
	}

	/* cleanup */
	receiver.dissolve(&context_123);
	destroy(env()->heap(), sender);
	for (int i = 0; i < NUM_HANDLERS; i++)
		destroy(env()->heap(), handler[i]);

	printf("TEST %d FINISHED\n", test_cnt);
}


/**
 * Stress test to estimate signal throughput
 *
 * For this test, we disable status output and any simulated wait times.
 * We produce and handle notifications as fast as possible via spinning
 * loops at the sender and handler side.
 */
static void stress_test()
{
	enum { SPEED            = 10 };
	enum { DURATION_SECONDS = 5  };
	enum { FINISH_IDLE_TIME = 100*SPEED };

	printf("\n");
	printf("TEST %d: stress test, busy signal transmission and handling\n", ++test_cnt);
	printf("\n");

	Signal_receiver receiver;
	Id_signal_context context_123(123);

	Handler *handler = new (env()->heap()) Handler(&receiver, 0, false);
	Sender  *sender  = new (env()->heap()) Sender(receiver.manage(&context_123),
	                                              0, false);

	for (int i = 1; i <= DURATION_SECONDS; i++) {
		printf("%d/%d\n", i, DURATION_SECONDS);
		timer.msleep(1000);
	}

	/* stop emitting signals */
	printf("deactivate sender\n");
	sender->idle();

	while (handler->receive_cnt() < sender->submit_cnt()) {
		printf("waiting for signals still in flight...");
		timer.msleep(FINISH_IDLE_TIME);
	}

	printf("\n");
	printf("sender submitted a total of %d signals\n", sender->submit_cnt());
	printf("handler received a total of %d signals\n", handler->receive_cnt());
	printf("\n");
	printf("processed %d notifications per second\n",
	       (handler->receive_cnt())/DURATION_SECONDS);
	printf("handler was activated %d times per second\n",
	       (handler->activation_cnt())/DURATION_SECONDS);
	printf("\n");

	if (sender->submit_cnt() != handler->receive_cnt())
		throw Test_failed_with_unequal_sent_and_received_signals();

	receiver.dissolve(&context_123);
	destroy(env()->heap(), sender);
	destroy(env()->heap(), handler);

	printf("TEST %d FINISHED\n", test_cnt);
}


static void lazy_receivers_test()
{
	printf("\n");
	printf("TEST %d: lazy and out-of-order signal reception test\n", ++test_cnt);
	printf("\n");

	Signal_receiver rec_1, rec_2;
	Signal_context rec_context_1, rec_context_2;

	Signal_transmitter transmitter_1(rec_1.manage(&rec_context_1));
	Signal_transmitter transmitter_2(rec_2.manage(&rec_context_2));

	printf("submit and receive signals with multiple receivers in order\n");
	transmitter_1.submit();
	transmitter_2.submit();

	{
		Signal signal = rec_1.wait_for_signal();
		printf("returned from wait_for_signal for receiver 1\n");

		signal = rec_2.wait_for_signal();
		printf("returned from wait_for_signal for receiver 2\n");
	}

	printf("submit and receive signals with multiple receivers out of order\n");
	transmitter_1.submit();
	transmitter_2.submit();

	{
		Signal signal = rec_2.wait_for_signal();
		printf("returned from wait_for_signal for receiver 2\n");

		signal = rec_1.wait_for_signal();
		printf("returned from wait_for_signal for receiver 1\n");
	}

	rec_1.dissolve(&rec_context_1);
	rec_2.dissolve(&rec_context_2);

	printf("TEST %d FINISHED\n", test_cnt);
}


/**
 * Try correct initialization and cleanup of receiver/context
 */
static void check_context_management()
{
	Id_signal_context         *context;
	Signal_receiver           *rec;
	Signal_context_capability  cap;

	/* setup receiver side */
	context = new (env()->heap()) Id_signal_context(321);
	rec     = new (env()->heap()) Signal_receiver;
	cap     = rec->manage(context);

	/* spawn sender */
	Sender *sender = new (env()->heap()) Sender(cap, 500);

	/* stop sender after timeout */
	timer.msleep(1000);
	printf("suspend sender\n");
	sender->idle();

	/* collect pending signals and dissolve context from receiver */
	{
		Signal signal = rec->wait_for_signal();
		printf("got %d signal(s) from %p\n", signal.num(), signal.context());
	}
	rec->dissolve(context);

	/* let sender spin for some time */
	printf("resume sender\n");
	sender->idle(false);
	timer.msleep(1000);
	printf("suspend sender\n");
	sender->idle();

	printf("destroy sender\n");
	destroy(env()->heap(), sender);
}


/**
 * Test if 'Signal_receiver::dissolve()' blocks as long as the signal context
 * is still referenced by one or more 'Signal' objects
 */

static Lock signal_context_destroyer_lock(Lock::LOCKED);
static bool signal_context_destroyed = false;

class Signal_context_destroyer : public Thread<4096>
{
	private:

		Signal_receiver *_receiver;
		Signal_context  *_context;

	public:

		Signal_context_destroyer(Signal_receiver *receiver, Signal_context *context)
		: Thread("signal_context_destroyer"),
		  _receiver(receiver), _context(context) { }

		void entry()
		{
			signal_context_destroyer_lock.lock();
			_receiver->dissolve(_context);
			signal_context_destroyed = true;
			destroy(env()->heap(), _context);
		}
};

static void synchronized_context_destruction_test()
{
	Signal_receiver receiver;

	Signal_context *context = new (env()->heap()) Signal_context;

	Signal_transmitter transmitter(receiver.manage(context));
	transmitter.submit();

	Signal_context_destroyer signal_context_destroyer(&receiver, context);
	signal_context_destroyer.start();

	/* The signal context destroyer thread should not be able to destroy the
	 * signal context during the 'Signal' objects life time. */
	{
		Signal signal = receiver.wait_for_signal();

		/* let the signal context destroyer thread try to destroy the signal context */
		signal_context_destroyer_lock.unlock();
		timer.msleep(1000);

		Signal signal_copy = signal;
		Signal signal_copy2 = signal;

		signal_copy = signal_copy2;

		if (signal_context_destroyed) {
			PERR("signal context destroyed too early");
			sleep_forever();
		}
	}

	signal_context_destroyer.join();
}


/**
 * Main program
 */
int main(int, char **)
{
	printf("--- signalling test ---\n");

	fast_sender_test();
	multiple_handlers_test();
	stress_test();
	lazy_receivers_test();
	check_context_management();
	synchronized_context_destruction_test();

	printf("--- signalling test finished ---\n");
	return 0;
}
