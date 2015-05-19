/*
 * \brief   Event that is provided by akernel thread-object for user handling
 * \author  Martin Stein
 * \date    2013-11-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

namespace Kernel
{
	class Thread;

	/**
	 * Event that is provided by kernel thread-objects for user handling
	 */
	class Thread_event;
}

class Kernel::Thread_event : public Signal_ack_handler
{
	private:

		Thread * const   _thread;
		Signal_context * _signal_context;


		/************************
		 ** Signal_ack_handler **
		 ************************/

		void _signal_acknowledged();

	public:

		/**
		 * Constructor
		 *
		 * \param t  thread that blocks on the event
		 */
		Thread_event(Thread * const t);

		/**
		 * Submit to listening handlers just like a signal context
		 */
		void submit();

		/**
		 * Kernel name of assigned signal context or 0 if not assigned
		 */
		Signal_context * const signal_context() const;

		/**
		 * Override signal context of the event
		 *
		 * \param c  new signal context or 0 to dissolve current signal context
		 */
		void signal_context(Signal_context * const c);
};
