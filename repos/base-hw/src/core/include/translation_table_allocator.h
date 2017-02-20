/*
 * \brief  Translation table allocator
 * \author Stefan Kalkowski
 * \date   2015-06-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_H_
#define _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_H_

#include <base/allocator.h>

namespace Genode {

	/**
	 * Translation table allocator interface
	 */
	class Translation_table_allocator;
}


class Genode::Translation_table_allocator : public Genode::Allocator
{
	public:

		/**
		 * Return physical address of given virtual page address
		 *
		 * \param addr  virtual page address
		 */
		virtual void * phys_addr(void * addr) = 0;

		/**
		 * Return virtual address of given physical page address
		 *
		 * \param addr  physical page address
		 */
		virtual void * virt_addr(void * addr) = 0;
};

#endif /* _CORE__INCLUDE__TRANSLATION_TABLE_ALLOCATOR_H_ */
