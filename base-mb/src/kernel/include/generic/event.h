/*
 * \brief  Event throwers and listeners
 * \author Martin Stein
 * \date   2010-10-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__EVENT_H_
#define _KERNEL__INCLUDE__GENERIC__EVENT_H_

#include <util/queue.h>
#include <generic/verbose.h> 

namespace Kernel {

	class Event
	{
		public:

			class Listener;
			typedef Kernel::Queue<Listener> Listener_queue;

			enum On_occurence__result{ EVENT_PROCESSED, EVENT_PENDING };

		private:

			Listener_queue _listeners;
			Listener      *_first;

		protected:

			void _populate()
			{
				if (!_first) {
					_first = _listeners.dequeue();
					if (!_first)
						return;
				}
				Listener *i = _first;

				while (1) {
					i->_on_event();
					_listeners.enqueue(i);
					i = _listeners.dequeue();
					if (i == _first) break;
				}
			}

			void _add(Listener* l) { _listeners.enqueue(l); }

			void _remove(Listener* l) { _listeners.remove(l); }

			void print_listeners()
			{
				printf("print_listeners\n");

				if (_listeners.empty()) {
					printf("  empty\n");
					return; }

				Listener *current;
				Listener *first;
				first   = _listeners.head();
				current = _listeners.dequeue();
				printf("  ");

				while (1) {
					printf("0x%p", current);
					_listeners.enqueue(current);
					if (first == _listeners.head())
						break;
					current = _listeners.dequeue();
					printf(" → "); }

				printf("\n");
			}

		public:

			virtual ~Event() { }

			class Listener : public Listener_queue::Item
			{
				friend class Event;

				protected:

					virtual void _on_event() = 0;

				public:

					virtual ~Listener(){}
			};
	};
}

#endif  /*_KERNEL__INCLUDE__GENERIC__EVENT_H_*/
