/*
 * \brief  Test pseudo-parallel use of FPU if available
 * \author Martin Stein
 * \date   2014-04-29
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/log.h>

using namespace Genode;

class Sync_signal_transmitter : public Signal_transmitter
{
	private:

		Lock _lock;

	public:

		Sync_signal_transmitter(Signal_context_capability context = Signal_context_capability())
		:
			Signal_transmitter(context),
			_lock(Lock::UNLOCKED)
		{ }

		void submit(unsigned cnt)
		{
			Lock::Guard guard(_lock);
			Signal_transmitter::submit(cnt);
		}
};

class Fpu_user : public Thread_deprecated<0x2000>
{
	private:

		float _x;
		Sync_signal_transmitter * _st;

		void _calc(float volatile & x, float volatile & y)
		{
			for (unsigned j = 0; j < 100; j++) {
				x *= (y * 1.357);
				x /= (y * 1.246);
			}
		}

	public:

		Fpu_user() : Thread_deprecated("fpu_user"), _x(0), _st(0) { }

		void start(float const x, Sync_signal_transmitter * const st)
		{
			_x = x;
			_st = st;
			Thread::start();
		}

		void entry()
		{
			Genode::log("FPU user started");
			bool submitted = false;
			while (1) {
				enum { TRIALS = 1000 };
				for (unsigned i = 0; i < TRIALS; i++) {
					float volatile a = _x + (float)i * ((float)1 / TRIALS);
					float volatile b = _x + (float)i * ((float)1 / TRIALS);
					float volatile c = _x;
					_calc(a, c);
					_calc(b, c);
					if (a != b) {
						Genode::error("calculation error");
						_st->submit(1);
						sleep_forever();
					}
				}
				if (!submitted) {
					_st->submit(1);
					submitted = true;
				}
			}
		}
};

int main()
{
	/* create ack signal */
	Signal_context sc;
	Signal_receiver sr;
	Signal_context_capability const scc = sr.manage(&sc);
	Sync_signal_transmitter st(scc);

	/* start pseudo-parallel FPU users */
	enum { FPU_USERS = 10 };
	Fpu_user fpu_users[FPU_USERS];
	for (unsigned i = 0; i < FPU_USERS; i++) {
		float const x = (i + 1) * 1.234;
		fpu_users[i].start(x, &st);
	}
	/* wait for an ack of every FPU user */
	for (unsigned i = 0; i < FPU_USERS;) { i += sr.wait_for_signal().num(); }
	log("test done");
	sleep_forever();
	return 0;
}
