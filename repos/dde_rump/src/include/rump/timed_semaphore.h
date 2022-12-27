/*
 * \brief  Semaphore implementation with timeout facility
 * \author Christian Prochaska
 * \date   2022-04-06
 *
 * This semaphore implementation allows to block on a semaphore for a
 * given time instead of blocking indefinitely.
 *
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RUMP__TIMED_SEMAPHORE_H_
#define _INCLUDE__RUMP__TIMED_SEMAPHORE_H_

#include <base/entrypoint.h>
#include <timer_session/connection.h>

class Ep_blockade
{
	private:

		Genode::Entrypoint &_ep;

		Genode::Io_signal_handler<Ep_blockade> _wakeup_handler
		{ _ep, *this, &Ep_blockade::_handle_wakeup };

		bool _signal_handler_called { false };

		void _handle_wakeup()
		{
			_signal_handler_called = true;
		}

	public:

		Ep_blockade(Genode::Entrypoint &ep) : _ep(ep) { }

		void block()
		{
			while (!_signal_handler_called)
				_ep.wait_and_dispatch_one_io_signal();

			_signal_handler_called = false;
		}

		void wakeup()
		{
			_wakeup_handler.local_submit();
		}
};


struct Timed_semaphore_blockade
{
	virtual void block() = 0;
	virtual void wakeup() = 0;
};


class Timed_semaphore_ep_blockade : public Timed_semaphore_blockade
{
	private:

		Ep_blockade _blockade;

	public:

		Timed_semaphore_ep_blockade(Genode::Entrypoint &ep)
		: _blockade(ep) { }
	
		void block() override
		{
			_blockade.block();
		}

		void wakeup() override
		{
			_blockade.wakeup();
		}
};


class Timed_semaphore_thread_blockade : public Timed_semaphore_blockade
{
	private:

		Genode::Blockade _blockade;

	public:

		Timed_semaphore_thread_blockade() { }
	
		void block() override
		{
			_blockade.block();
		}

		void wakeup() override
		{
			_blockade.wakeup();
		}
};


class Timed_semaphore
{
	public:

		struct Down_ok { };
		struct Down_timed_out { };
		using Down_result = Genode::Attempt<Down_ok, Down_timed_out>;

	private:

		Genode::Env &_env;
		Genode::Thread const *_ep_thread_ptr;
		Timer::Connection &_timer;

		int           _cnt;
		Genode::Mutex _meta_mutex { };

		struct Element : Genode::Fifo<Element>::Element
		{
			Timed_semaphore_blockade &blockade;
			int                      &cnt;
			Genode::Mutex            &meta_mutex;
			Genode::Fifo<Element>    &queue;

			Genode::Mutex            destruct_mutex { };
			Timer::One_shot_timeout<Element> timeout;
			bool wakeup_called { false };

			void handle_timeout(Genode::Duration)
			{
				{
					Genode::Mutex::Guard guard(meta_mutex);

					/*
					 * If 'wakeup()' was called, 'Timed_semaphore::up()'
					 * has already taken care of this.
					 */

					if (!wakeup_called) {

						cnt++;

						/*
						 * Remove element from queue so that a future 'up()'
						 * does not select it for wakeup.
						 */
						queue.remove(*this);
					}
				}

				/*
				 * Protect the 'blockade' member from destruction
				 * until 'blockade.wakeup()' has returned.
				 */
				Genode::Mutex::Guard guard(destruct_mutex);

				blockade.wakeup();
			}

			Element(Timed_semaphore_blockade &blockade,
			        int &cnt,
			        Genode::Mutex &meta_mutex,
			        Genode::Fifo<Element> &queue,
			        Timer::Connection &timer,
			        bool use_timeout = false,
			        Genode::Microseconds timeout_us = Genode::Microseconds(0))
			: blockade(blockade),
			  cnt(cnt),
			  meta_mutex(meta_mutex),
			  queue(queue),
			  timeout(timer, *this, &Element::handle_timeout)
			{
				if (use_timeout)
					timeout.schedule(timeout_us);
			}

			~Element()
			{
				/*
				 * Synchronize destruction with unfinished
				 * 'handle_timeout()' or 'wakeup()'
				 */
				Genode::Mutex::Guard guard(destruct_mutex);
			}

			Down_result block()
			{
				blockade.block();

				if (wakeup_called)
					return Down_ok();
				else
					return Down_timed_out();
			}

			/* meta_mutex must be acquired when calling and is released */
			void wakeup()
			{
				/*
				 * It is possible that 'handle_timeout()' is already being
				 * called and waiting for the meta_mutex, so in addition to
				 * discarding the timeout, the 'wakeup_called' variable is
				 * set for 'handle_timeout()' (and for 'block()').
				 */

				wakeup_called = true;

				meta_mutex.release();

				/*
				 * 'timeout.discard()' waits until an ongoing signal
				 * handler execution is finished, so meta_mutex must
				 * be released at this point.
				 */
				timeout.discard();

				/*
				 * Protect the 'blockade' member from destruction
				 * until 'blockade.wakeup()' has returned.
				 */
				Genode::Mutex::Guard guard(destruct_mutex);

				blockade.wakeup();
			}
		};

		Genode::Fifo<Element> _queue { };

		/* _meta_mutex must be acquired when calling and is released */
		Down_result _down_internal(Timed_semaphore_blockade &blockade,
		                           bool use_timeout,
		                           Genode::Microseconds timeout_us)
		{
			/*
			 * Create semaphore queue element representing the thread
			 * in the wait queue and release _meta_mutex.
			 */
			Element queue_element { blockade, _cnt, _meta_mutex, _queue,
			                        _timer, use_timeout, timeout_us };
			_queue.enqueue(queue_element);
			_meta_mutex.release();

			/*
			 * The thread is going to block now,
			 * waiting for getting woken up from another thread
			 * calling 'up()' or by the timeout handler.
			 */
			return queue_element.block();
		}

	public:


		/**
		 * Constructor
		 *
		 * \param env   Genode environment
		 * \param timer timer connection
		 * \param n     initial counter value of the semphore
		 *
		 * Note: currently it is assumed that the constructor is called
		 *       by the ep thread.
		 */
		Timed_semaphore(Genode::Env &env, Genode::Thread const *ep_thread_ptr,
		                Timer::Connection &timer, int n = 0)
		: _env(env), _ep_thread_ptr(ep_thread_ptr),
		  _timer(timer), _cnt(n) { }

		~Timed_semaphore()
		{
			/* synchronize destruction with unfinished 'up()' */
			try { _meta_mutex.acquire(); } catch (...) { }
		}

		/**
		 * Increment semaphore counter
		 *
		 * This method may wake up another thread that currently blocks on
		 * a 'down' call at the same semaphore.
		 */
		void up()
		{
			Element * element = nullptr;

			_meta_mutex.acquire();

			if (++_cnt > 0) {
				_meta_mutex.release();
				return;
			}

			/*
			 * Remove element from queue and wake up the corresponding
			 * blocking thread
			 */
			_queue.dequeue([&element] (Element &head) {
				element = &head; });

			if (element) {
				/* 'element->wakeup()' releases the _meta_mutex */
				element->wakeup();
			} else
				_meta_mutex.release();
		}

		/**
		 * Decrement semaphore counter, block if the counter reaches zero
		 */
		Down_result down(bool use_timeout = false,
		                 Genode::Microseconds timeout_us = Genode::Microseconds(0))
		{
			if (use_timeout && (timeout_us.value == 0))
				return Down_timed_out();

			_meta_mutex.acquire();

			if (--_cnt < 0) {

				/* _down_internal() releases _meta_mutex */

				if (Genode::Thread::myself() == _ep_thread_ptr) {
					Timed_semaphore_ep_blockade blockade { _env.ep() };
					return _down_internal(blockade, use_timeout, timeout_us);
				} else {
					Timed_semaphore_thread_blockade blockade;
					return _down_internal(blockade, use_timeout, timeout_us);
				}

			} else {
				_meta_mutex.release();
				return Down_ok();
			}
		}
};

#endif /* _INCLUDE__RUMP__TIMED_SEMAPHORE_H_ */
