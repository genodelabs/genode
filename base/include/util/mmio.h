/*
 * \brief  Generic MMIO access framework
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE__INCLUDE__UTIL__MMIO_H_
#define _BASE__INCLUDE__UTIL__MMIO_H_

#include <util/register.h>
#include <base/printf.h>


namespace Genode
{
	/**
	 * A continuous MMIO region
	 */
	class Mmio
	{
		protected:

			/**
			 * Write typed 'value' to MMIO base + 'o'
			 */
			template <typename STORAGE_T>
			inline void _write(off_t const o, STORAGE_T const value);

			/**
			 * Read typed from MMIO base + 'o'
			 */
			template <typename STORAGE_T>
			inline STORAGE_T _read(off_t const o) const;

		public:

			enum { BYTE_WIDTH_LOG2 = 3, BYTE_WIDTH = 1 << BYTE_WIDTH_LOG2 };

			/**
			 * A POD-like region at offset 'MMIO_OFFSET' within a MMIO region
			 *
			 * \detail  The register can contain multiple bitfields. Bitfields
			 *          that are partially out of the register range are read and 
			 *          written also partially. Bitfields that are completely out 
			 *          of the register range are read as '0' and trying to 
			 *          overwrite them has no effect
			 */
			template <off_t MMIO_OFFSET, typename STORAGE_T>
			struct Register : public Genode::Register<STORAGE_T>
			{
				enum { OFFSET = MMIO_OFFSET };

				/**
				 * A bitregion within a register
				 * 
				 * \detail  Bitfields are read and written according to their range,
				 *          so if we have a 'Bitfield<2,3>' and write '0b11101' to it
				 *          only '0b101' (shiftet by 2 bits) is written
				 */
				template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE>
				struct Bitfield : public Genode::Register<STORAGE_T>::template Bitfield<BIT_SHIFT, BIT_SIZE>
				{
					/**
					 * Back reference to containing register
					 */
					typedef Register<OFFSET, STORAGE_T> Compound_reg;
				};
			};

			/**
			 * An array of successive similar items
			 *
			 * \detail  'STORAGE_T' width must be a power of 2. The array
			 *          takes all bitfields that are covered by an item width and
			 *          iterates them successive 'ITEMS' times. Thus bitfields out of 
			 *          the range of the first item are not accessible in any item. 
			 *          Furthermore the maximum item width is the width of 'STORAGE_T'.
			 *          The array is not limited to one 'STORAGE_T' instance,
			 *          it uses as much successive instances as needed.
			 */
			template <off_t MMIO_OFFSET, typename STORAGE_T, unsigned long ITEMS, unsigned long _ITEM_WIDTH_LOG2>
			struct Register_array : public Register<MMIO_OFFSET, STORAGE_T>
			{
				enum {
					MAX_INDEX       = ITEMS - 1,
					ITEM_WIDTH_LOG2 = _ITEM_WIDTH_LOG2,
					ITEM_WIDTH      = 1 << ITEM_WIDTH_LOG2,
					ITEM_MASK       = (1 << ITEM_WIDTH) - 1,
					ITEMS_PER_REG   = (sizeof(STORAGE_T) << BYTE_WIDTH_LOG2) >> ITEM_WIDTH_LOG2,
					ITEM_IS_REG     = ITEMS_PER_REG == 1,
					STORAGE_WIDTH   = BYTE_WIDTH * sizeof(STORAGE_T),

					/**
					 * Static sanity check
					 */
					ERROR_ITEMS_OVERSIZED = ITEMS_PER_REG / ITEMS_PER_REG,
				};

				/**
				 * A bitregion within a register array
				 *
				 * \detail  Bitfields are only written and read as far as the
				 *          item width reaches, i.e. assume a 'Register_array<0,uint8_t,6,2>'
				 *          that contains a 'Bitfield<1,5>' and we write '0b01101' to it, then
				 *          only '0b101' is written and read in consequence. Bitfields that are
				 *          completely out of the item range ar read as '0' and trying to overwrite 
				 *          them has no effect.
				 */
				template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE>
				struct Bitfield : public Register<MMIO_OFFSET, STORAGE_T>::template Bitfield<BIT_SHIFT, BIT_SIZE>
				{
					/**
					 * Back reference to containing register array
					 */
					typedef Register_array<MMIO_OFFSET, STORAGE_T, ITEMS, ITEM_WIDTH_LOG2> Compound_array;
				};

				/**
				 * Calculate the MMIO-relative offset 'offset' and shift 'shift'
				 * within the according 'storage_t' instance to access this bitfield 
				 * from item 'index'
				 */
				static inline void access_dest(off_t & offset, unsigned long & shift,
				                               unsigned long const index)
				{
					unsigned long const bit_off = index << ITEM_WIDTH_LOG2;
					offset  = (off_t) ((bit_off >> BYTE_WIDTH_LOG2) & ~(sizeof(STORAGE_T)-1) );
					shift   = bit_off - ( offset << BYTE_WIDTH_LOG2 );
					offset += MMIO_OFFSET;
				}
			};

			addr_t const base;

			/**
			 * Constructor
			 */
			inline Mmio(addr_t mmio_base) : base(mmio_base) { }


			/*************************
			 ** Access to registers **
			 *************************/

			/**
			 * Typed address of register 'REGISTER'
			 */
			template <typename REGISTER>
			inline typename REGISTER::storage_t volatile * typed_addr() const;

			/**
			 * Read the whole register 'REGISTER'
			 */
			template <typename REGISTER>
			inline typename REGISTER::storage_t read() const;

			/**
			 * Write 'value' to the register 'REGISTER'
			 */
			template <typename REGISTER>
			inline void write(typename REGISTER::storage_t const value);


			/******************************************
			 ** Access to bitfields within registers **
			 ******************************************/

			/**
			 * Read the bitfield 'BITFIELD'
			 */
			template <typename BITFIELD>
			inline typename BITFIELD::Compound_reg::storage_t read() const;

			/**
			 * Write value to the bitfield 'BITFIELD'
			 */
			template <typename BITFIELD>
			inline void write(typename BITFIELD::Compound_reg::storage_t const value);


			/*******************************
			 ** Access to register arrays **
			 *******************************/

			/**
			 * Read the whole item 'index' of the array 'REGISTER_ARRAY'
			 */
			template <typename REGISTER_ARRAY>
			inline typename REGISTER_ARRAY::storage_t read(unsigned long const index) const;

			/**
			 * Write 'value' to item 'index' of the array 'REGISTER_ARRAY' 
			 */
			template <typename REGISTER_ARRAY>
			inline void write(typename REGISTER_ARRAY::storage_t const value,
			                  unsigned long const index);


			/*****************************************************
			 ** Access to bitfields within register array items **
			 *****************************************************/

			/**
			 * Read the bitfield 'ARRAY_BITFIELD' of item 'index' of the compound reg array
			 */
			template <typename ARRAY_BITFIELD>
			inline typename ARRAY_BITFIELD::Compound_array::storage_t read(unsigned long const index) const;

			/**
			 * Write 'value' to bitfield 'ARRAY_BITFIELD' of item 'index' of the compound reg array
			 */
			template <typename ARRAY_BITFIELD>
			inline void write(typename ARRAY_BITFIELD::Compound_array::storage_t const value, long unsigned const index);
	};
}


template <typename STORAGE_T>
void Genode::Mmio::_write(off_t const o, STORAGE_T const value)
{
	*(STORAGE_T volatile *)((addr_t)base + o) = value;
}


template <typename STORAGE_T>
STORAGE_T Genode::Mmio::_read(off_t const o) const
{
	return *(STORAGE_T volatile *)((addr_t)base + o);
}


/*************************
 ** Access to registers **
 *************************/


template <typename REGISTER>
typename REGISTER::storage_t volatile * Genode::Mmio::typed_addr() const
{
	return (typename REGISTER::storage_t volatile *)base + REGISTER::OFFSET;
}


template <typename REGISTER>
typename REGISTER::storage_t Genode::Mmio::read() const
{
	return _read<typename REGISTER::storage_t>(REGISTER::OFFSET);
}


template <typename REGISTER>
void Genode::Mmio::write(typename REGISTER::storage_t const value)
{
	_write<typename REGISTER::storage_t>(REGISTER::OFFSET, value);
}


/******************************************
 ** Access to bitfields within registers **
 ******************************************/


template <typename BITFIELD>
typename BITFIELD::Compound_reg::storage_t Genode::Mmio::read() const
{
	typedef typename BITFIELD::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	return BITFIELD::get(_read<storage_t>(Register::OFFSET));
}


template <typename BITFIELD>
void Genode::Mmio::write(typename BITFIELD::Compound_reg::storage_t const value)
{
	typedef typename BITFIELD::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	storage_t new_reg = read<Register>();
	BITFIELD::clear(new_reg);
	BITFIELD::set(new_reg, value);

	write<Register>(new_reg);
}


/************************************
 ** Access to register array items **
 ************************************/


template <typename REGISTER_ARRAY>
typename REGISTER_ARRAY::storage_t Genode::Mmio::read(unsigned long const index) const
{
	/**
	 * Handle array overflow
	 */
	if (index > REGISTER_ARRAY::MAX_INDEX) return 0;

	off_t offset;

	/**
	 * Optimize access if item width equals storage type width
	 */
	if (REGISTER_ARRAY::ITEM_IS_REG) {

		offset = REGISTER_ARRAY::OFFSET + (index << REGISTER_ARRAY::ITEM_WIDTH_LOG2);
		return _read<typename REGISTER_ARRAY::storage_t>(offset);

	} else {

		long unsigned shift;
		REGISTER_ARRAY::access_dest(offset, shift, index);
		return (_read<typename REGISTER_ARRAY::storage_t>(offset) >> shift) & REGISTER_ARRAY::ITEM_MASK;
	}

}


template <typename REGISTER_ARRAY>
void Genode::Mmio::write(typename REGISTER_ARRAY::storage_t const value,
                         unsigned long const index)
{
	/**
	 * Avoid array overflow
	 */
	if (index > REGISTER_ARRAY::MAX_INDEX) return;

	off_t offset;

	/**
	 * Optimize access if item width equals storage type width
	 */
	if (REGISTER_ARRAY::ITEM_IS_REG) {

		offset = REGISTER_ARRAY::OFFSET + (index << REGISTER_ARRAY::ITEM_WIDTH_LOG2);
		_write<typename REGISTER_ARRAY::storage_t>(offset, value);

	} else {

		long unsigned shift;
		REGISTER_ARRAY::access_dest(offset, shift, index);

		/**
		 * Insert new value into old register value
		 */
		typename REGISTER_ARRAY::storage_t new_reg = _read<typename REGISTER_ARRAY::storage_t>(offset);
		new_reg &= ~(REGISTER_ARRAY::ITEM_MASK << shift);
		new_reg |= (value & REGISTER_ARRAY::ITEM_MASK) << shift;

		_write<typename REGISTER_ARRAY::storage_t>(offset, new_reg);
	}
}


/*****************************************************
 ** Access to bitfields within register array items **
 *****************************************************/


template <typename ARRAY_BITFIELD>
void Genode::Mmio::write(typename ARRAY_BITFIELD::Compound_array::storage_t const value, 
                         long unsigned const index)
{
	typedef typename ARRAY_BITFIELD::Compound_array Register_array;

	typename Register_array::storage_t new_reg = read<Register_array>(index);
	ARRAY_BITFIELD::clear(new_reg);
	ARRAY_BITFIELD::set(new_reg, value);

	write<Register_array>(new_reg, index);
}


template <typename ARRAY_BITFIELD>
typename ARRAY_BITFIELD::Compound_array::storage_t Genode::Mmio::read(long unsigned const index) const
{
	typedef typename ARRAY_BITFIELD::Compound_array Array;
	typedef typename Array::storage_t storage_t;

	return ARRAY_BITFIELD::get(read<Array>(index));
}


#endif /* _BASE__INCLUDE__UTIL__MMIO_H_ */

