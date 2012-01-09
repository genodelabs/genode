/*
 * \brief  Generic accessor framework for highly structured memory regions
 * \author Martin stein
 * \date   2011-11-10
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
		struct Subreg
		{
			enum {
				SHIFT = BIT_SHIFT,
				WIDTH = BIT_SIZE,
				MASK  = (1 << WIDTH) - 1,
			};

			/**
			 * Back reference to containing register
			 */
			typedef Register<storage_t> Compound_reg;

			/**
			 * Get a register value with this subreg set to 'value'
			 * and the rest left zero
			 */
			static storage_t bits(storage_t const value) { return (value & MASK) << SHIFT; }

			/**
			 * Get value of this subreg from 'reg'
			 */
			static storage_t value(storage_t const reg) { return (reg >> SHIFT) & MASK; }

			/**
			 * Get registervalue 'reg' with this subreg set to zero
			 */
			static void clear_bits(storage_t & reg) { reg &= ~(MASK << SHIFT); }

			/**
			 * Get registervalue 'reg' with this subreg set to 'value'
			 */
			static void set_bits(storage_t & reg, storage_t const value = ~0)
			{
				clear_bits(reg);
				reg |= (value & MASK) << SHIFT;
			};
		};
	};
}

#endif /* _BASE__INCLUDE__UTIL__CPU_REGISTER_H_ */

