/*
 * \brief  Test the distribution and application of CPU quota
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/component.h>
#include <base/sleep.h>
#include <timer_session/connection.h>

/* local includes */
#include <sync_session/connection.h>

using namespace Genode;

struct Single_signal
{
		Signal_receiver           receiver;
		Signal_context            context;
		Signal_context_capability cap;
		Signal_transmitter        transmitter;

		Single_signal() : cap(receiver.manage(&context)), transmitter(cap) { }

		~Single_signal() { receiver.dissolve(&context); }
		void receive()   { receiver.wait_for_signal(); }
		void submit()    { transmitter.submit(); }
};


struct Synchronizer
{
	Single_signal  signal;
	Sync::Session &session;

	Synchronizer(Sync::Session &session) : session(session) { }

	void threshold(unsigned threshold) { session.threshold(threshold); }

	void synchronize()
	{
		session.submit(signal.cap);
		signal.receive();
	}
};


class Counter : public Thread
{
	private:

		enum { STACK_SIZE = 2 * 1024 * sizeof(addr_t) };

		enum Stage { PAUSE, MEASUREMENT, DESTRUCTION };

		Name               const    &_name;
		unsigned long long volatile  _value { 0 };
		Stage              volatile  _stage { PAUSE };
		Single_signal                _start_measurement;
		Single_signal                _start_destruction;
		Synchronizer                 _synchronizer;

		void entry()
		{
			unsigned long long volatile value = 0;
			while (_stage == PAUSE) {
				_start_measurement.receive();
				_stage = MEASUREMENT;
				_synchronizer.synchronize();
				while (_stage == MEASUREMENT) { value++; }
			}
			_value = value;
			_start_destruction.submit();
		}

	public:

		Counter(Env &env, Name const &name, unsigned cpu_percent, Sync::Session &sync)
		:
			Thread(env, name, STACK_SIZE, Location(),
			       Weight(Cpu_session::quota_lim_upscale(cpu_percent, 100)),
			       env.cpu()),
			_name(name), _synchronizer(sync) { start(); }

		void destruct()
		{
			_stage = DESTRUCTION;
			_start_destruction.receive();
			this->~Counter();
		}

		void pause()   { _stage = PAUSE; }
		void measure() { _start_measurement.submit(); }

		void print(Output &output) const { Genode::print(output, _name, " ", _value); }
};


struct Main
{
	enum { DURATION_BASE_SEC           = 20,
	       MEASUREMENT_1_NR_OF_THREADS = 9,
	       MEASUREMENT_2_NR_OF_THREADS = 6,
	       CONCLUSION_NR_OF_THREADS    = 3, };

	Env                 &env;
	Single_signal        timer_signal;
	Timer::Connection    timer        { env };
	Sync::Connection     sync         { env };
	Synchronizer         synchronizer { sync };
	Counter::Name const  name_a       { "counter A" };
	Counter::Name const  name_b       { "counter B" };
	Counter              counter_a    { env, name_a, 10, sync };
	Counter              counter_b    { env, name_b, 90, sync };

	Main(Env &env) : env(env)
	{
		Cpu_session::Quota quota = env.cpu().quota();
		log("quota super period ", quota.super_period_us);
		log("quota ", quota.us);
		log("start measurement ...");
		timer.sigh(timer_signal.cap);

		auto measure = [&] (unsigned duration_sec) {
			timer.trigger_once(duration_sec * 1000 * 1000);
			synchronizer.synchronize();
			timer_signal.receive();
		};
		/* measurement 1 */
		synchronizer.threshold(MEASUREMENT_1_NR_OF_THREADS);
		counter_a.measure();
		counter_b.measure();
		measure(3 * DURATION_BASE_SEC);
		counter_a.pause();
		counter_b.destruct();

		/* measurement 2 */
		synchronizer.threshold(MEASUREMENT_2_NR_OF_THREADS);
		counter_a.measure();
		measure(DURATION_BASE_SEC);
		counter_a.destruct();

		/* conclusion */
		synchronizer.threshold(CONCLUSION_NR_OF_THREADS);
		synchronizer.synchronize();
		log(counter_a);
		log(counter_b);
		log("done");
	}
};


void Component::construct(Env &env) { static Main main(env); }
