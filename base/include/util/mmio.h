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

/* Genode includes */
#include <util/register.h>

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
			template <typename _ACCESS_T>
			inline void _write(off_t const o, _ACCESS_T const value) {
				*(_ACCESS_T volatile *)((addr_t)base + o) = value; }

			/**
			 * Read typed from MMIO base + 'o'
			 */
			template <typename _ACCESS_T>
			inline _ACCESS_T _read(off_t const o) const {
				return *(_ACCESS_T volatile *)((addr_t)base + o); }

		public:

			enum { BYTE_WIDTH_LOG2 = 3, BYTE_WIDTH = 1 << BYTE_WIDTH_LOG2 };

			/**
			 * An integer like region within a MMIO region.
			 *
			 * \param  _OFFSET        Offset of the region relative to the
			 *                        base of the compound MMIO
			 * \param  _ACCESS_WIDTH  Bit width of the region, for a list of
			 *                        supported widths see 'Genode::Register'
			 * \param  _STRICT_WRITE  If set to 0, when writing a bitfield, we
			 *                        read the register value, update the bits
			 *                        on it, and write it back to the register.
			 *                        If set to 1 we take an empty register
			 *                        value instead, apply the bitfield on it,
			 *                        and write it to the register. This can
			 *                        be useful if you have registers that have
			 *                        different means on reads and writes.
			 *
			 * \detail  See 'Genode::Register'
			 */
			template <off_t _OFFSET, unsigned long _ACCESS_WIDTH,
			          bool _STRICT_WRITE = false>
			struct Register : public Genode::Register<_ACCESS_WIDTH>
			{
				enum {
					OFFSET       = _OFFSET,
					ACCESS_WIDTH = _ACCESS_WIDTH,
					STRICT_WRITE = _STRICT_WRITE,
				};

				/**
				 * A region within a register
				 *
				 * \param  _SHIFT  Bit shift of the first bit within the
				 *                 compound register
				 * \param  _WIDTH  Bit width of the region
				 *
				 * \detail  See 'Genode::Register::Bitfield'
				 */
				template <unsigned long _SHIFT, unsigned long _WIDTH>
				struct Bitfield :
					public Genode::Register<ACCESS_WIDTH>::template Bitfield<_SHIFT, _WIDTH>
				{
					/* Back reference to containing register */
					typedef Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE> Compound_reg;
				};
			};

			/**
			 * An array of successive equally structured regions
			 *
			 * \param  _OFFSET        Offset of the first region relative to
			 *                        the base of the compound MMIO.
			 * \param  _ACCESS_WIDTH  Bit width of a single access, must be at
			 *                        least the item width.
			 * \param  _ITEMS         How many times the region gets iterated
			 *                        successive
			 * \param  _ITEM_WIDTH    Bit width of a region
			 * \param  _STRICT_WRITE  If set to 0, when writing a bitfield, we
			 *                        read the register value, update the bits
			 *                        on it, and write it back to the register.
			 *                        If set to 1, we take an empty register
			 *                        value instead, apply the bitfield on it,
			 *                        and write it to the register. This can
			 *                        be useful if you have registers that have
			 *                        different means on reads and writes.
			 *                        Please note that ACCESS_WIDTH is decisive
			 *                        for the range of such strictness.
			 *
			 * \detail  The array takes all inner structures, wich are covered
			 *          by an item width and iterates them successive. Such
			 *          structures that are partially exceed an item range are
			 *          read and written also partially. Structures that are
			 *          completely out of the item range are read as '0' and
			 *          trying to overwrite them has no effect. The array is
			 *          not limited to its access width, it extends to the
			 *          memory region of its successive items. Trying to read
			 *          out read with an item index out of the array range
			 *          returns '0', trying to write to such indices has no
			 *          effect
			 */
			template <off_t _OFFSET, unsigned long _ACCESS_WIDTH,
			          unsigned long _ITEMS, unsigned long _ITEM_WIDTH,
			          bool _STRICT_WRITE = false>

			struct Register_array : public Register<_OFFSET, _ACCESS_WIDTH,
			                                        _STRICT_WRITE>
			{
				typedef
				typename Trait::Uint_type<_ACCESS_WIDTH>::template Divisor<_ITEM_WIDTH>
				Item;

				enum {
					STRICT_WRITE    = _STRICT_WRITE,
					OFFSET          = _OFFSET,
					ACCESS_WIDTH    = _ACCESS_WIDTH,
					ITEMS           = _ITEMS,
					ITEM_WIDTH      = _ITEM_WIDTH,

					ITEM_WIDTH_LOG2 = Item::WIDTH_LOG2,
					MAX_INDEX       = ITEMS - 1,
					ITEM_MASK       = (1 << ITEM_WIDTH) - 1,
				};

				typedef
				typename Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE>::access_t
				access_t;

				/**
				 * A bitregion within a register array item
				 *
				 * \param  _SHIFT  Bit shift of the first bit within an item
				 * \param  _WIDTH  Bit width of the region
				 *
				 * \detail  See 'Genode::Register::Bitfield'
				 */
				template <unsigned long _SHIFT, unsigned long _SIZE>
				struct Bitfield :
					public Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE>::template Bitfield<_SHIFT, _SIZE>
				{
					/* Back reference to containing register array */
					typedef Register_array<OFFSET, ACCESS_WIDTH, ITEMS,
					                       ITEM_WIDTH, STRICT_WRITE>
						Compound_array;
				};

				/**
				 * Calculate destination of an array-item access
				 *
				 * \param  offset  Gets overridden with the offset of the
				 *                 access type instance, that contains the
				 *                 access destination, relative to the MMIO
				 *                 base
				 * \param  shift   Gets overridden with the shift of the
				 *                 destination within the access type instance
				 *                 targeted by 'offset'
				 * \param  index   Index of the targeted array item
				 */
				static inline void access_dest(off_t & offset,
				                               unsigned long & shift,
				                               unsigned long const index)
				{
					unsigned long const bit_off = index << ITEM_WIDTH_LOG2;
					offset  = (off_t) ((bit_off >> BYTE_WIDTH_LOG2)
					          & ~(sizeof(access_t)-1) );
					shift   = bit_off - ( offset << BYTE_WIDTH_LOG2 );
					offset += OFFSET;
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
			inline typename REGISTER::access_t volatile * typed_addr() const {
				return (typename REGISTER::access_t volatile *)
					base + REGISTER::OFFSET; }

			/**
			 * Read the whole register 'REGISTER'
			 */
			template <typename REGISTER>
			inline typename REGISTER::access_t read() const {
				return _read<typename REGISTER::access_t>(REGISTER::OFFSET); }

			/**
			 * Write 'value' to the register 'REGISTER'
			 */
			template <typename REGISTER>
			inline void write(typename REGISTER::access_t const value) {
				_write<typename REGISTER::access_t>(REGISTER::OFFSET, value); }


			/******************************************
			 ** Access to bitfields within registers **
			 ******************************************/

			/**
			 * Read the bitfield 'BITFIELD'
			 */
			template <typename BITFIELD>
			inline typename BITFIELD::Compound_reg::access_t read() const;

			/**
			 * Write value to the bitfield 'BITFIELD'
			 */
			template <typename BITFIELD>
			inline void
			write(typename BITFIELD::Compound_reg::access_t const value);


			/*******************************
			 ** Access to register arrays **
			 *******************************/

			/**
			 * Read the whole item 'index' of the array 'REGISTER_ARRAY'
			 */
			template <typename REGISTER_ARRAY>
			inline typename REGISTER_ARRAY::access_t
			read(unsigned long const index) const;

			/**
			 * Write 'value' to item 'index' of the array 'REGISTER_ARRAY'
			 */
			template <typename REGISTER_ARRAY>
			inline void write(typename REGISTER_ARRAY::access_t const value,
			                  unsigned long const index);


			/*****************************************************
			 ** Access to bitfields within register array items **
			 *****************************************************/

			/**
			 * Read the bitfield 'ARRAY_BITFIELD' of item 'index' of the
			 * compound reg array
			 */
			template <typename ARRAY_BITFIELD>
			inline typename ARRAY_BITFIELD::Compound_array::access_t
			read(unsigned long const index) const;

			/**
			 * Write 'value' to bitfield 'ARRAY_BITFIELD' of item 'index' of
			 * the compound reg array
			 */
			template <typename ARRAY_BITFIELD>
			inline void
			write(typename ARRAY_BITFIELD::Compound_array::access_t const value,
			      long unsigned const index);


			/*********************************
			 ** Polling for bitfield states **
			 *********************************/

			/**
			 * Interface for delaying the execution of a calling thread
			 */
			struct Delayer
			{
				/**
				 * Delay the execution of the caller for the specified amount
				 * of microseconds
				 */
				virtual void usleep(unsigned us) = 0;
			};

			/**
			 * Wait until the 'BITFIELD' contains the specified 'value'
			 *
			 * \param value         value to wait for
			 * \param delayer       sleeping facility to be used when the
			 *                      value is not reached yet
			 * \param max_attempts  number of bitfield probing attempts
			 * \param us            number of microseconds between attempts
			 */
			template <typename BITFIELD>
			inline bool
			wait_for(typename BITFIELD::Compound_reg::access_t const value,
			         Delayer &delayer,
			         unsigned max_attempts = 500,
			         unsigned us           = 1000)
			{
				for (unsigned i = 0; i < max_attempts; i++, delayer.usleep(us))
					if (read<BITFIELD>() == value)
						return true;

				return false;
			}
	};
}


/******************************************
 ** Access to bitfields within registers **
 ******************************************/


template <typename BITFIELD>
typename BITFIELD::Compound_reg::access_t Genode::Mmio::read() const
{
	typedef typename BITFIELD::Compound_reg Register;
	typedef typename Register::access_t access_t;
	return BITFIELD::get(_read<access_t>(Register::OFFSET));
}


template <typename BITFIELD>
void Genode::Mmio::write(typename BITFIELD::Compound_reg::access_t const value)
{
	/* Initialize the pattern that is written finally to the register */
	typedef typename BITFIELD::Compound_reg Register;
	typename Register::access_t write_value;
	if (Register::STRICT_WRITE)
	{
		/* We must only apply the bitfield to an empty write pattern */
		write_value = 0;
	} else {

		/* We've got to apply the bitfield to the old register value */
		write_value = read<Register>();
		BITFIELD::clear(write_value);
	}
	/* Apply bitfield value and override register */
	BITFIELD::set(write_value, value);
	write<Register>(write_value);
}


/************************************
 ** Access to register array items **
 ************************************/


template <typename REGISTER_ARRAY>
typename REGISTER_ARRAY::access_t
Genode::Mmio::read(unsigned long const index) const
{
	/* Reads outside the array return 0 */
	if (index > REGISTER_ARRAY::MAX_INDEX) return 0;

	/* If item width equals access width we optimize the access */
	off_t offset;
	if (REGISTER_ARRAY::ITEM_WIDTH == REGISTER_ARRAY::ACCESS_WIDTH) {
		offset = REGISTER_ARRAY::OFFSET
		         + (index << REGISTER_ARRAY::ITEM_WIDTH_LOG2);
		return _read<typename REGISTER_ARRAY::access_t>(offset);

	/* Access width and item width differ */
	} else {
		long unsigned shift;
		REGISTER_ARRAY::access_dest(offset, shift, index);
		return (_read<typename REGISTER_ARRAY::access_t>(offset) >> shift)
		       & REGISTER_ARRAY::ITEM_MASK;
	}

}


template <typename REGISTER_ARRAY>
void Genode::Mmio::write(typename REGISTER_ARRAY::access_t const value,
                         unsigned long const index)
{
	/* Writes outside the array are ignored */
	if (index > REGISTER_ARRAY::MAX_INDEX) return;

	/* If item width equals access width we optimize the access */
	off_t offset;
	if (REGISTER_ARRAY::ITEM_WIDTH == REGISTER_ARRAY::ACCESS_WIDTH) {
		offset = REGISTER_ARRAY::OFFSET
		         + (index << REGISTER_ARRAY::ITEM_WIDTH_LOG2);
		_write<typename REGISTER_ARRAY::access_t>(offset, value);

	/* Access width and item width differ */
	} else {
		long unsigned shift;
		REGISTER_ARRAY::access_dest(offset, shift, index);

		/* Insert new value into old register value */
		typename REGISTER_ARRAY::access_t write_value;
		if (REGISTER_ARRAY::STRICT_WRITE)
		{
			/* We must only apply the bitfield to an empty write pattern */
			write_value = 0;
		} else {

			/* We've got to apply the bitfield to the old register value */
			write_value = _read<typename REGISTER_ARRAY::access_t>(offset);
			write_value &= ~(REGISTER_ARRAY::ITEM_MASK << shift);
		}
		/* Apply bitfield value and override register */
		write_value |= (value & REGISTER_ARRAY::ITEM_MASK) << shift;
		_write<typename REGISTER_ARRAY::access_t>(offset, write_value);
	}
}


/*****************************************************
 ** Access to bitfields within register array items **
 *****************************************************/


template <typename ARRAY_BITFIELD>
void
Genode::Mmio::write(typename ARRAY_BITFIELD::Compound_array::access_t const value,
                    long unsigned const index)
{
	/* Initialize the pattern that is finally written to the register */
	typedef typename ARRAY_BITFIELD::Compound_array Register_array;
	typename Register_array::access_t write_value;
	if (Register_array::STRICT_WRITE)
	{
		/* We must only apply the bitfield to an empty write pattern */
		write_value = 0;
	} else {

		/* We've got to apply the bitfield to the old register value */
		write_value = read<Register_array>(index);
		ARRAY_BITFIELD::clear(write_value);
	}
	/* Apply bitfield value and override register */
	ARRAY_BITFIELD::set(write_value, value);
	write<Register_array>(write_value, index);
}


template <typename ARRAY_BITFIELD>
typename ARRAY_BITFIELD::Compound_array::access_t
Genode::Mmio::read(long unsigned const index) const
{
	typedef typename ARRAY_BITFIELD::Compound_array Array;
	typedef typename Array::access_t access_t;
	return ARRAY_BITFIELD::get(read<Array>(index));
}


#endif /* _BASE__INCLUDE__UTIL__MMIO_H_ */

