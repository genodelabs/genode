/*
 * \brief  Test the distribution and application of CPU quota
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <timer_session/connection.h>
#include <sync_session/connection.h>

using namespace Genode;

enum { SYNC_SIG = 0 };

namespace Sync { class Signal; }

class Single_signal
{
	private:

		Signal_receiver           _sigr;
		Signal_context            _sigx;
		Signal_context_capability _sigc;
		Signal_transmitter        _sigt;

	public:

		Single_signal() : _sigc(_sigr.manage(&_sigx)), _sigt(_sigc) { }

		~Single_signal() { _sigr.dissolve(&_sigx); }

		void receive() { _sigr.wait_for_signal(); }

		void submit() { _sigt.submit(); }

		operator Signal_context_capability() { return _sigc; }
};

class Sync::Signal
{
	private:

		Signal_receiver           _sigr;
		Signal_context            _sigx;
		Signal_context_capability _sigc;
		Session * const           _session;
		unsigned const            _id;

	public:

		Signal(Session * const session, unsigned const id)
		: _sigc(_sigr.manage(&_sigx)), _session(session), _id(id) { }

		~Signal() { _sigr.dissolve(&_sigx); }

		void threshold(unsigned const threshold) {
			_session->threshold(_id, threshold); }

		void sync()
		{
			_session->submit(_id, _sigc);
			_sigr.wait_for_signal();
		}
};

class Counter : private Thread_deprecated<2 * 1024 * sizeof(Genode::addr_t)>
{
	private:

		String<64>                  _name;
		unsigned long long volatile _value;
		Sync::Signal                _sync_sig;
		unsigned volatile           _stage;
		Single_signal               _stage_1_end;
		Single_signal               _stage_2_reached;

		inline void _stage_0_and_1(unsigned long long volatile & value)
		{
			_stage_1_end.receive();
			_stage = 0;
			_sync_sig.sync();
			while(_stage == 0) { value++; }
		}

		void entry()
		{
			unsigned long long volatile value = 0;
			while (_stage < 2) { _stage_0_and_1(value); }
			_value = value;
			_stage_2_reached.submit();
			sleep_forever();
		}

	public:

		Counter(char const *name, size_t const weight,
		        Sync::Session * const sync)
		:
			Thread_deprecated(weight, "counter"), _name(name), _value(0) ,
			_sync_sig(sync, SYNC_SIG), _stage(1)
		{
			Thread::start();
		}

		void destruct()
		{
			_stage = 2;
			_stage_2_reached.receive();
			this->~Counter();
		}

		void pause() { _stage = 1; }

		void go() { _stage_1_end.submit(); }

		void result() { log("counter ", _name, " ", _value); }
};


void measure(Timer::Connection & timer, Single_signal & timer_sig,
             Sync::Signal & sync_sig, unsigned const sec)
{
	timer.trigger_once(sec * 1000 * 1000);
	sync_sig.sync();
	timer_sig.receive();
}


int main()
{
	enum { DURATION_BASE_SEC = 20 };

	/* prepare */
	Single_signal     timer_sig;
	Timer::Connection timer;
	Sync::Connection  sync;
	Sync::Signal      sync_sig(&sync, SYNC_SIG);
	Counter           counter_a("A", Cpu_session::quota_lim_upscale(10, 100), &sync);
	Counter           counter_b("B", Cpu_session::quota_lim_upscale(90, 100), &sync);

	timer.sigh(timer_sig);

	/* measure stage 1 */
	sync_sig.threshold(9);
	counter_a.go();
	counter_b.go();
	measure(timer, timer_sig, sync_sig, 3 * DURATION_BASE_SEC);
	counter_a.pause();
	counter_b.destruct();

	/* measure stage 2 */
	sync_sig.threshold(6);
	counter_a.go();
	measure(timer, timer_sig, sync_sig, DURATION_BASE_SEC);
	counter_a.destruct();

	/* print results */
	sync_sig.threshold(3);
	sync_sig.sync();
	Cpu_session::Quota quota = Genode::env()->cpu_session()->quota();
	log("quota super period ", quota.super_period_us);
	log("quota ", quota.us);
	counter_a.result();
	counter_b.result();
	log("done");
	sleep_forever();
}
