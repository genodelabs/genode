/*
 * \brief  Queue with first-in first-out semantics
 * \author Norman Feske
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2019 Genode Labs GmbH
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
 * First-in first-out (FIFO) queue
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

				QT  *_next     { nullptr };
				bool _enqueued { false   };

			public:

				Element() { }

				/**
				 * Return true is fifo element is enqueued in a fifo
				 */
				bool enqueued() const { return _enqueued; }

				/**
				 * Return next element in queue
				 *
				 * \deprecated  use 'Fifo::for_each' instead
				 */
				QT *next() const { return _next; }
		};

	private:

		QT      *_head { nullptr };  /* oldest element */
		Element *_tail { nullptr };  /* newest element */

	public:

		/**
		 * Return true if queue is empty
		 */
		bool empty() const { return _tail == nullptr; }

		/**
		 * Constructor
		 */
		Fifo() { }

		/**
		 * Call 'func' of type 'void (QT&)' the head element
		 */
		template <typename FUNC>
		void head(FUNC const &func) const { if (_head) func(*_head); }

		/**
		 * Remove element explicitly from queue
		 */
		void remove(QT &qe)
		{
			if (empty()) return;

			/* if specified element is the first of the queue */
			if (&qe == _head) {
				_head = qe.Fifo::Element::_next;
				if (!_head)
					_tail = nullptr;

			} else {

				/* search specified element in the queue */
				Element *e = _head;
				while (e->_next && (e->_next != &qe))
					e = e->_next;

				/* element is not member of the queue */
				if (!e->_next) return;

				/* e->_next is the element to remove, skip it in list */
				e->Fifo::Element::_next = e->Fifo::Element::_next->Fifo::Element::_next;
				if (!e->Element::_next) _tail = e;
			}

			qe.Fifo::Element::_next = nullptr;
			qe.Fifo::Element::_enqueued = false;
		}

		/**
		 * Attach element at the end of the queue
		 */
		void enqueue(QT &e)
		{
			e.Fifo::Element::_next = nullptr;
			e.Fifo::Element::_enqueued = true;

			if (empty()) {
				_tail = _head = &e;
				return;
			}

			_tail->Fifo::Element::_next = &e;
			_tail = &e;
		}

		/**
		 * Call 'func' of type 'void (QT&)' for each element in order
		 */
		template <typename FUNC>
		void for_each(FUNC const &func) const
		{
			QT *elem = _head;
			while (elem != nullptr) {
				/* take the next pointer so 'func' cannot modify it */
				QT *next = elem->Fifo::Element::_next;;
				func(*elem);
				elem = next;
			}
		}

		/**
		 * Remove head and call 'func' of type 'void (QT&)'
		 */
		template <typename FUNC>
		void dequeue(FUNC const &func)
		{
			QT *result = _head;

			/* check if queue has only one last element */
			if (_head == _tail) {
				_head = nullptr;
				_tail = nullptr;
			} else
				_head = _head->Fifo::Element::_next;

			/* mark fifo queue element as free */
			if (result) {
				result->Fifo::Element::_next = nullptr;
				result->Fifo::Element::_enqueued = false;

				/* pass to caller */
				func(*result);
			}
		}

		/**
		 * Remove all fifo elements
		 *
		 * This method removes all elements in order and calls the lambda
		 * 'func' of type 'void (QT&)' for each element. It is intended to be
		 * used prior the destruction of the FIFO.
		 */
		template <typename FUNC>
		void dequeue_all(FUNC const &func)
		{
			while (_head != nullptr) dequeue(func);
		}
};


/**
 * Helper for using member variables as FIFO elements
 *
 * \param T  type of compound object to be organized in a FIFO
 *
 * This helper allows the creation of FIFOs that use member variables to
 * connect their elements. This way, the organized type does not need to
 * publicly inherit 'Fifo<QT>::Element'. Furthermore, objects can easily
 * be organized in multiple FIFOs by embedding multiple 'Fifo_element'
 * member variables.
 */
template <typename T>
class Genode::Fifo_element : public Fifo<Fifo_element<T> >::Element
{
	T &_object;

	public:

		Fifo_element(T &object) : _object(object) { }

		T       &object()       { return _object; }
		T const &object() const { return _object; }

};

#endif /* _INCLUDE__UTIL__FIFO_H_ */
