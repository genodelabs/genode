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

#ifndef _INCLUDE__UTIL__FIFO_H_
#define _INCLUDE__UTIL__FIFO_H_

namespace Genode {

	/*
	 * \param QT  queue element type
	 */
	template <typename QT>
	class Fifo
	{
		private:

			QT *_head;  /* oldest element */
			QT *_tail;  /* newest element */

		public:

			class Element
			{
				protected:

					friend class Fifo;

					QT *_next;
					bool _is_enqueued;

				public:

					Element(): _next(0), _is_enqueued(false) { }

					/**
					 * Return true is fifo element is enqueued in a fifo
					 */
					bool is_enqueued() { return _is_enqueued; }
			};

		public:

			/**
			 * Return true if queue is empty
			 */
			bool empty() { return _tail == 0; }

			/**
			 * Constructor
			 *
			 * Start with an empty list.
			 */
			Fifo(): _head(0), _tail(0) { }

			/**
			 * Attach element at the end of the queue
			 */
			void enqueue(QT *e)
			{
				e->Fifo::Element::_next = 0;
				e->Fifo::Element::_is_enqueued = true;

				if (empty()) {
					_tail = _head = e;
					return;
				}

				_tail->Fifo::Element::_next = e;
				_tail = e;
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
					_head = _head->Fifo::Element::_next;

				/* mark fifo queue element as free */
				if (result) {
					result->Fifo::Element::_next = 0;
					result->Fifo::Element::_is_enqueued = false;
				}

				return result;
			}
	};
}

#endif /* _INCLUDE__UTIL__FIFO_H_ */
