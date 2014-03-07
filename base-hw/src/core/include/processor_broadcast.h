/*
 * \brief   Utility to execute a function on all available processors
 * \author  Martin Stein
 * \date    2014-03-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_BROADCAST_H_
#define _PROCESSOR_BROADCAST_H_

/* Genode includes */
#include <base/thread.h>
#include <base/signal.h>

namespace Genode {

	enum { PROCESSOR_BROADCAST_RECEIVER_STACK_SIZE = 4 * 1024 };

	/**
	 * Functionality that can be broadcasted on all available processors
	 */
	class Processor_broadcast_operation;

	/**
	 * Wrapper for the signalling between broadcast and receiver
	 */
	class Processor_broadcast_signal;

	/**
	 * Processor local receiver of broadcasted functions
	 */
	class Processor_broadcast_receiver;

	/**
	 * Executes a function on all available processors
	 */
	class Processor_broadcast;

	/**
	 * Return broadcast singleton
	 */
	Processor_broadcast * processor_broadcast();
}

class Genode::Processor_broadcast_operation
{
	public:

		typedef void (*Entry)(void * const);

	private:

		Entry const  _entry;
		void * const _data;

	public:

		/**
		 * Constructor
		 *
		 * \param entry  entry to the operation code
		 * \param data   pointer to operation specific input/output data
		 */
		Processor_broadcast_operation(Entry const entry, void * const data)
		:
			_entry(entry), _data(data)
		{ }

		/**
		 * Execute operation processor-locally
		 */
		void execute() const { _entry(_data); }
};

class Genode::Processor_broadcast_signal
{
	private:

		Signal_context     _context;
		Signal_receiver    _receiver;
		Signal_transmitter _transmitter;

	public:

		/**
		 * Constructor
		 */
		Processor_broadcast_signal()
		:
			_transmitter(_receiver.manage(&_context))
		{ }

		/**
		 * Submit the signal
		 */
		void submit() { _transmitter.submit(); }

		/**
		 * Wait for signal submission
		 */
		void await() { _receiver.wait_for_signal(); }
};

class Genode::Processor_broadcast_receiver
:
	public Thread<PROCESSOR_BROADCAST_RECEIVER_STACK_SIZE>
{
	private:

		typedef Processor_broadcast_operation Operation;
		typedef Processor_broadcast_signal    Signal;

		Operation const * _operation;
		Signal            _start;
		Signal            _end;

	public:

		/**
		 * Constructor
		 */
		Processor_broadcast_receiver() : Thread("processor_broadcast") { }

		/**
		 * Start receiver on a specific processor
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		void init(unsigned const processor_id)
		{
			Thread::utcb()->core_start_info()->init(processor_id);
			Thread::start();
		}

		/**
		 * Start remote execution of an operation
		 *
		 * \param operation  desired operation
		 */
		void start_executing(Operation const * const operation)
		{
			_operation = operation;
			_start.submit();
		}

		/**
		 * Wait until the remote execution of the current operation is done
		 */
		void end_executing() { _end.await(); }


		/*****************
		 ** Thread_base **
		 *****************/

		void entry()
		{
			while (1) {
				_start.await();
				_operation->execute();
				_end.submit();
			}
		}
};

class Genode::Processor_broadcast
{
	private:

		typedef Processor_broadcast_operation Operation;
		typedef Processor_broadcast_receiver  Receiver;

		Receiver _receiver[PROCESSORS];

	public:

		/**
		 * Constructor
		 */
		Processor_broadcast()
		{
			for (unsigned i = 0; i < PROCESSORS; i++) { _receiver[i].init(i); }
		}

		/**
		 * Execute operation on all available processors
		 *
		 * \param operation  desired operation
		 */
		void execute(Operation const * const operation)
		{
			for (unsigned i = 0; i < PROCESSORS; i++) {
				_receiver[i].start_executing(operation);
			}
			for (unsigned i = 0; i < PROCESSORS; i++) {
				_receiver[i].end_executing();
			}
		}
};

#endif /* _PROCESSOR_BROADCAST_H_ */
