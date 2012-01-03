/*
 * \brief  Queue with first-in first-out semantics
 * \author Norman Feske
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__QUEUE_H_
#define _INCLUDE__UTIL__QUEUE_H_

namespace Kernel {

	/*
	 * \param QT  queue element type
	 */
	template <typename QT>
	class Queue
	{
		protected:

			QT *_head;  /* oldest element */
			QT *_tail;  /* newest element */

		public:

			class Item
			{
				protected:

					friend class Queue;

					QT *_next;

				public:

					Item() : _next(0) {}

					~Item() { }

//					/**
//					 * Return true is fifo element is enqueued in a fifo
//					 */
//					bool is_enqueued() { return _next != 0; }
			};

		public:

			QT* head(){ return _head; }

			/**
			 * Return true if queue is empty
			 */
			bool empty() { return _tail == 0; }

			/**
			 * Constructor
			 *
			 * Start with an empty list.
			 */
			Queue(): _head(0), _tail(0)  { }

			/**
			 * Destructor
			 */
			virtual ~Queue() { }

			/**
			 * Attach element at the end of the queue
			 */
			void enqueue(QT *e)
			{
				e->_next = 0;

				if (empty())
					_tail = _head = e;
				else {
					_tail->_next = e;
					_tail = e;
				}
				_enqueue__verbose__success();
}

			/**
			 * Obtain head element of the queue and remove element from queue
			 *
			 * \return  head element or 0 if queue is empty
			 */
			QT *dequeue()
			{
				QT *result = _head;

				/* check if queue has only one last element */
				if (_head == _tail)
					_head = _tail = 0;
				else
					_head = _head->_next;

				/* mark fifo queue element as free */
				if (result)
					result->_next = 0;

				return result;
			}

			/**
			 * Remove element from queue if it is enqueued
			 */
			void remove(QT *e)
			{
				QT* current=_head;
				QT* predecessor=0;

				if (current!=e){
					while (1){
						predecessor=current;
						current=current->_next;
						if (current==e || !current)
							break;
					}
				} else {
					dequeue();
					return;
				}

				if (!current) return;
				if (current==_tail) _tail=predecessor;

				predecessor->_next=e->_next;
				e->_next=0;
}

		protected:

			virtual void _enqueue__verbose__success(){}

	};
}

#endif /* _INCLUDE__UTIL__QUEUE_H_ */
