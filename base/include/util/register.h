/*
 * \brief  Generic access framework for highly structured memory regions
 * \author Martin stein
 * \date   2011-11-10
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__INCLUDE__UTIL__REGISTER_H_
#define _BASE__INCLUDE__UTIL__REGISTER_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * A POD-like highly structured memory region
	 */
	template <typename STORAGE_T>
	struct Register
	{
		typedef STORAGE_T storage_t;

		/**
		 * A bitregion within a register
		 */
		template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE>
		struct Bitfield
		{
			enum {
				SHIFT      = BIT_SHIFT,
				WIDTH      = BIT_SIZE,
				MASK       = (1 << WIDTH) - 1,
				REG_MASK   = MASK << SHIFT,
				CLEAR_MASK = ~REG_MASK,
			};

			/**
			 * Back reference to containing register
			 */
			typedef Register<storage_t> Compound_reg;

			/**
			 * Get a register value with this bitfield set to 'value' and the rest left zero
			 *
			 * \detail  Useful to combine successive access to multiple bitfields
			 *          into one operation
			 */
			static inline storage_t bits(storage_t const value) { return (value & MASK) << SHIFT; }

			/**
			 * Get value of this bitfield from 'reg'
			 */
			static inline storage_t get(storage_t const reg) { return (reg >> SHIFT) & MASK; }

			/**
			 * Get registervalue 'reg' with this bitfield set to zero
			 */
			static inline void clear(storage_t & reg) { reg &= CLEAR_MASK; }

			/**
			 * Get registervalue 'reg' with this bitfield set to 'value'
			 */
			static inline void set(storage_t & reg, storage_t const value = ~0)
			{
				clear(reg);
				reg |= (value & MASK) << SHIFT;
			};
		};
	};
}

#endif /* _BASE__INCLUDE__UTIL__REGISTER_H_ */

