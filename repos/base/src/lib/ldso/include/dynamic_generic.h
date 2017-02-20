/**
 * \brief  Generic dynamic section address handling
 * \author Sebastian Sumpf
 * \date   2016-02-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DYNAMIC_GENERIC_H_
#define _INCLUDE__DYNAMIC_GENERIC_H_

extern const unsigned long _GLOBAL_OFFSET_TABLE_[] __attribute__((visibility("hidden")));
extern unsigned long       _DYNAMIC[] __attribute__((visibility("hidden")));

namespace Linker {

	/**
	 * Address of .dynamic section in GOT
	 */
	static inline unsigned long dynamic_address_got()
	{
		return _GLOBAL_OFFSET_TABLE_[0];
	}

	/**
	 * Address of .dynamic section from symbol
	 */
	static inline unsigned long dynamic_address()
	{
		return (unsigned long)&_DYNAMIC;
	}

	/**
	 * Return the run-time load address of the shared object.
	 */
	static inline unsigned long relocation_address(void)
	{
		return dynamic_address() < dynamic_address_got() ?
		       dynamic_address_got() - dynamic_address() :
		       dynamic_address() - dynamic_address_got();
	}
}

#endif /* _INCLUDE__DYNAMIC_GENERIC_H_ */
