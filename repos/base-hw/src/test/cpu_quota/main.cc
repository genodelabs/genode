/*
 * \brief  Diversified test of the Register and MMIO framework
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
#include <base/printf.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>
#include <timer_session/connection.h>

using namespace Genode;

class My_thread : public Thread<8 * 1024>
{
	private:

		Signal_receiver * const _sigr;
		bool volatile           _stop;

	public:

		My_thread(Signal_receiver * const sigr)
		: Thread(Cpu_session::pc_to_quota(100), "counter"),
		  _sigr(sigr), _stop(0) { }

		void entry()
		{
			_sigr->wait_for_signal();
			unsigned volatile i = 0;
			while(!_stop) { i++; }
			printf("%u\n", i);
			sleep_forever();
		}

		void stop() { _stop = 1; }
};

int main()
{
	Timer::Connection timer;
	Signal_receiver sigr;
	Signal_context sigx;
	Signal_context_capability sigc = sigr.manage(&sigx);
	Signal_transmitter sigt(sigc);
	My_thread thread(&sigr);
	thread.start();
	timer.msleep(3000);
	sigt.submit();
	timer.msleep(30000);
	thread.stop();
	sleep_forever();
}

