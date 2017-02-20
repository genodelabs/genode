/*
 * \brief  Queue with first-in first-out semantics
 * \author Norman Feske
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__FIFO_H_
#define _INCLUDE__UTIL__FIFO_H_

namespace Genode {
	
	template<typename> class Fifo;
	template <typename> class Fifo_element;
}


/**
 * Fifo queue
 *
 * \param QT  queue element type
 */
template <typename QT>
class Genode::Fifo
{
	public:

		class Element
		{
			protected:

				friend class Fifo;

				QT  *_next;
				bool _enqueued;

			public:

				Element(): _next(0), _enqueued(false) { }

				/**
				 * Return true is fifo element is enqueued in a fifo
				 */
				bool enqueued() { return _enqueued; }

				/**
				 * Return true is fifo element is enqueued in a fifo
				 *
				 * \noapi
				 * \deprecated  use 'enqueued' instead
				 */
				bool is_enqueued() { return enqueued(); }

				/**
				 * Return next element in queue
				 */
				QT *next() const { return _next; }
		};

	private:

		QT      *_head;  /* oldest element */
		Element *_tail;  /* newest element */

	public:

		/**
		 * Return true if queue is empty
		 */
		bool empty() { return _tail == 0; }

		/**
		 * Constructor
		 */
		Fifo(): _head(0), _tail(0) { }

		/**
		 * Return first queue element
		 */
		QT *head() const { return _head; }

		/**
		 * Remove element explicitely from queue
		 */
		void remove(QT *qe)
		{
			if (empty()) return;

			/* if specified element is the first of the queue */
			if (qe == _head) {
				_head = qe->Element::_next;
				if (!_head) _tail  = 0;
			}
			else {

				/* search specified element in the queue */
				Element *e = _head;
				while (e->_next && (e->_next != qe))
					e = e->_next;

				/* element is not member of the queue */
				if (!e->_next) return;

				/* e->_next is the element to remove, skip it in list */
				e->Element::_next = e->Element::_next->Element::_next;
				if (!e->Element::_next) _tail = e;
			}

			qe->Element::_next = 0;
			qe->Element::_enqueued = 0;
		}

		/**
		 * Attach element at the end of the queue
		 */
		void enqueue(QT *e)
		{
			e->Fifo::Element::_next = 0;
			e->Fifo::Element::_enqueued = true;

			if (empty()) {
				_tail = _head = e;
				return;
			}

			_tail->Fifo::Element::_next = e;
			_tail = e;
		}

		/**
		 * Remove head element from queue
		 *
		 * \return  head element or 0 if queue is empty
		 */
		QT *dequeue()
		{
			QT *result = _head;

			/* check if queue has only one last element */
			if (_head == _tail) {
				_head = 0;
				_tail = 0;
			} else
				_head = _head->Fifo::Element::_next;

			/* mark fifo queue element as free */
			if (result) {
				result->Fifo::Element::_next = 0;
				result->Fifo::Element::_enqueued = false;
			}

			return result;
		}
};


/**
 * Helper for using member variables as FIFO elements
 *
 * \param T  type of compound object to be organized in a FIFO
 *
 * This helper allow the creation of FIFOs that use member variables to
 * connect their elements. This way, the organized type does not need to
 * publicly inherit 'Fifo<QT>::Element'. Furthermore objects can easily
 * be organized in multiple FIFOs by embedding multiple 'Fifo_element'
 * member variables.
 */
template <typename T>
class Genode::Fifo_element : public Fifo<Fifo_element<T> >::Element
{
	T *_object;

	public:

		Fifo_element(T *object) : _object(object) { }

		/**
		 * Get typed object pointer
		 *
		 * Zero-pointer save: Returns 0 if this pointer is 0 to
		 * cover the case of accessing an empty FIFO.
		 */
		inline T *object()
		{
			if (this) { return _object; }
			return 0;
		}
};

#endif /* _INCLUDE__UTIL__FIFO_H_ */
