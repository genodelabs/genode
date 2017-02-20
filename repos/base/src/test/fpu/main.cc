/*
 * \brief  Test pseudo-parallel use of FPU if available
 * \author Martin Stein
 * \date   2014-04-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/thread.h>

using namespace Genode;

class Fpu_user : public Thread
{
	private:

		float              _x;
		Signal_transmitter _st;
		Semaphore &        _sem;

		void _calc(float volatile & x, float volatile & y)
		{
			for (unsigned j = 0; j < 100; j++) {
				x *= (y * 1.357);
				x /= (y * 1.246);
			}
		}

	public:

		Fpu_user(Env & env, float x, Signal_context_capability c, Semaphore &s)
		: Thread(env, "fpu_user", sizeof(size_t)*2048), _x(x), _st(c), _sem(s) {
			start(); }

		void entry()
		{
			log("FPU user started");

			enum { TRIALS = 1000 };
			for (unsigned i = 0; i < TRIALS; i++) {
				float volatile a = _x + (float)i * ((float)1 / TRIALS);
				float volatile b = _x + (float)i * ((float)1 / TRIALS);
				float volatile c = _x;
				_calc(a, c);
				_calc(b, c);
				if (a != b) {
					error("calculation error");
					break;
				}
			}

			_sem.up();
			_st.submit();
		}
};


struct Main
{
	enum { FPU_USERS = 10 };

	Semaphore            sem;
	Env &                env;
	Heap                 heap    { env.ram(), env.rm() };
	Signal_handler<Main> handler { env.ep(), *this, &Main::handle };

	Main(Env & env) : env(env) {
		for (unsigned i = 0; i < FPU_USERS; i++)
			new (heap) Fpu_user(env, (i + 1) * 1.234, handler, sem); }

	void handle() {
		if (sem.cnt() >= FPU_USERS) log("test done"); }
};


void Component::construct(Genode::Env & env) {
	static Main main(env); }
