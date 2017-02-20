/*
 * \brief   Queue with first-in first-out semantics
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__KERNEL__FIFO_H_
#define _CORE__INCLUDE__KERNEL__FIFO_H_

/* Genode includes */
#include <util/fifo.h>

namespace Kernel {

	/**
	 * Queue with first-in first-out semantics
	 *
	 * \param T  queue element type
	 */
	template <typename T>
	class Fifo : public Genode::Fifo<T>
	{
		public:

			/**
			 * Call function 'f' of type 'void (QT *)' for each queue element
			 */
			template <typename F> void for_each(F f)
			{
				typedef Genode::Fifo<T> B;
				for (T * e = B::head(); e; e = e->B::Element::next()) { f(e); }
			}
	};
}

#endif /* _CORE__INCLUDE__KERNEL__FIFO_H_ */
