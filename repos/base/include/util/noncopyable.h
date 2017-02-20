/*
 * \brief  Non-copyable objects design pattern.
 * \author Stefan Kalkowski
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__NONCOPYABLE_H_
#define _INCLUDE__UTIL__NONCOPYABLE_H_

namespace Genode { class Noncopyable; }


/**
 * Classes of objects not allowed to be copied, should inherit from this one.
 *
 * This class declares a private copy-constructor and assignment-operator.
 * It's sufficient to inherit private from this class, and let the compiler
 * detect any copy violations.
 */
class Genode::Noncopyable
{
	private:

		/**
		 * Constructor
		 */
		Noncopyable(const Noncopyable&);

		const Noncopyable& operator=(const Noncopyable&);

	protected:

		/**
		 * Constructor
		 *
		 * \noapi
		 */
		Noncopyable()  {}

		/**
		 * Destructor
		 *
		 * \noapi
		 */
		~Noncopyable() {}
};

#endif /* _INCLUDE__UTIL__NONCOPYABLE_H_ */
