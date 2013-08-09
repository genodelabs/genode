/*
 * \brief  Thread with configurable stack size
 * \author Christian Prochaska
 * \date   2008-06-11
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_QT_H_
#define _INCLUDE__BASE__THREAD_QT_H_

#include <base/env.h>
#include <base/printf.h>
#include <base/thread.h>

enum { DEFAULT_STACK_SIZE = 4096*100 };

namespace Genode {

	struct Thread_entry
	{
		virtual void entry() = 0;
	};


	class Thread_qt : public Thread_entry
	{
		private:

			class Genode_thread : Thread_base
			{
				private:

					Thread_entry *_thread_entry;

					/**
					 * Thread_base interface
					 */
					void entry() { _thread_entry->entry(); }

				public:

					Genode_thread(const char   *name,
					              size_t        stack_size,
					              Thread_entry *thread_entry)
					:
						Thread_base(name, stack_size),
						_thread_entry(thread_entry)
					{
						/* start Genode thread */
						start();
					}
			};

		protected:

			const char    *_name;
			unsigned int   _stack_size;
			Genode_thread *_thread;

		public:

			/**
			 * Constructor
			 *
			 * \param name   Thread name (for debugging)
			 */
			explicit Thread_qt(const char *name = "Qt <noname>")
			:
				_name(name),
				_stack_size(DEFAULT_STACK_SIZE),
				_thread(0) { }

			~Thread_qt()
			{
				if (_thread)
					destroy(env()->heap(), _thread);
			}

			/**
			 * Set the thread's stack size - don't call when the thread is running!
			 */
			bool set_stack_size(unsigned int stack_size)
			{
				/* error, if thread is already running */
				if (_thread)
					return false;

				_stack_size = stack_size;
				return true;
			}

			/**
			 * Start execution of the thread
			 */
			void start()
			{
				/* prevent double calls of 'start' */
				if (_thread) return;

				_thread = new (env()->heap()) Genode_thread(_name, _stack_size, this);
			}

			static Thread_base *myself()
			{
				return Thread_base::myself();
			}
	};
}

#endif /* _INCLUDE__BASE__THREAD_QT_H_ */
