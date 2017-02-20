/*
 * \brief  Generic MMIO access framework
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__MMIO_H_
#define _INCLUDE__UTIL__MMIO_H_

/* Genode includes */
#include <util/register.h>

namespace Genode { class Mmio; }


/**
 * A continuous MMIO region
 *
 * For correct behavior of the methods of 'Mmio', a class that
 * derives from one of the subclasses of 'Mmio' must not define members
 * named 'Register_base', 'Bitfield_base', 'Register_array_base' or
 * 'Array_bitfield_base'.
 */
class Genode::Mmio
{
	/**
	 * Write '_ACCESS_T' typed 'value' to MMIO base + 'o'
	 */
	template <typename _ACCESS_T>
	inline void _write(off_t const o, _ACCESS_T const value)
	{
		addr_t const dst = (addr_t)base + o;
		*(_ACCESS_T volatile *)dst = value;
	}

	/**
	 * Read '_ACCESS_T' typed from MMIO base + 'o'
	 */
	template <typename _ACCESS_T>
	inline _ACCESS_T _read(off_t const o) const
	{
		addr_t const dst = (addr_t)base + o;
		_ACCESS_T const value = *(_ACCESS_T volatile *)dst;
		return value;
	}

	public:

		enum { BYTE_WIDTH_LOG2 = 3, BYTE_WIDTH = 1 << BYTE_WIDTH_LOG2 };

		/**
		 * An integer like region within a MMIO region.
		 *
		 * \param _OFFSET        Offset of the region relative to the
		 *                       base of the compound MMIO.
		 * \param _ACCESS_WIDTH  Bit width of the region, for a list of
		 *                       supported widths see 'Genode::Register'.
		 * \param _STRICT_WRITE  If set to 0, when writing a bitfield, we
		 *                       read the register value, update the bits
		 *                       on it, and write it back to the register.
		 *                       If set to 1 we take an empty register
		 *                       value instead, apply the bitfield on it,
		 *                       and write it to the register. This can
		 *                       be useful if you have registers that have
		 *                       different means on reads and writes.
		 *
		 * For further details See 'Genode::Register'.
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

			/*
			 * GCC 4.4, in contrast to GCC versions >= 4.5, can't
			 * select function templates like 'write(typename
			 * T::Register::access_t value)' through a given 'T'
			 * that, in this case, derives from 'Register<X, Y, Z>'.
			 * It seems this is due to the fact that 'T::Register'
			 * is a template. Thus we provide some kind of stamp
			 * that solely must not be redefined by the deriving
			 * class to ensure correct template selection.
			 */
			typedef Register<_OFFSET, _ACCESS_WIDTH, _STRICT_WRITE>
				Register_base;

			/**
			 * A region within a register
			 *
			 * \param _SHIFT  Bit shift of the first bit within the
			 *                compound register.
			 * \param _WIDTH  bit width of the region
			 *
			 * For details see 'Genode::Register::Bitfield'.
			 */
			template <unsigned long _SHIFT, unsigned long _WIDTH>
			struct Bitfield : public Genode::Register<ACCESS_WIDTH>::
			                         template Bitfield<_SHIFT, _WIDTH>
			{
				/* analogous to 'Mmio::Register::Register_base' */
				typedef Bitfield<_SHIFT, _WIDTH> Bitfield_base;

				/* back reference to containing register */
				typedef Register<_OFFSET, _ACCESS_WIDTH, _STRICT_WRITE>
					Compound_reg;
			};
		};

		/**
		 * An array of successive equally structured regions, called items
		 *
		 * \param _OFFSET        Offset of the first item relative to
		 *                       the base of the compound MMIO.
		 * \param _ACCESS_WIDTH  Bit width of a single access, must be at
		 *                       least the item width.
		 * \param _ITEMS         How many times the item gets iterated
		 *                       successively.
		 * \param _ITEM_WIDTH    bit width of an item
		 * \param _STRICT_WRITE  If set to 0, when writing a bitfield, we
		 *                       read the register value, update the bits
		 *                       on it, and write it back to the register.
		 *                       If set to 1, we take an empty register
		 *                       value instead, apply the bitfield on it,
		 *                       and write it to the register. This can
		 *                       be useful if you have registers that have
		 *                       different means on reads and writes.
		 *                       Please note that ACCESS_WIDTH is decisive
		 *                       for the range of such strictness.
		 *
		 * The array takes all inner structures, wich are covered by an
		 * item width and iterates them successively. Such structures that
		 * are partially exceed an item range are read and written also
		 * partially. Structures that are completely out of the item range
		 * are read as '0' and trying to overwrite them has no effect. The
		 * array is not limited to its access width, it extends to the
		 * memory region of its successive items. Trying to read out read
		 * with an item index out of the array range returns '0', trying
		 * to write to such indices has no effect.
		 */
		template <off_t _OFFSET, unsigned long _ACCESS_WIDTH,
		          unsigned long _ITEMS, unsigned long _ITEM_WIDTH,
		          bool _STRICT_WRITE = false>

		struct Register_array : public Register<_OFFSET, _ACCESS_WIDTH,
		                                        _STRICT_WRITE>
		{
			typedef typename Trait::Uint_width<_ACCESS_WIDTH>::
			                 template Divisor<_ITEM_WIDTH> Item;

			enum {
				STRICT_WRITE    = _STRICT_WRITE,
				OFFSET          = _OFFSET,
				ACCESS_WIDTH    = _ACCESS_WIDTH,
				ITEMS           = _ITEMS,
				ITEM_WIDTH      = _ITEM_WIDTH,
				ITEM_WIDTH_LOG2 = Item::WIDTH_LOG2,
				MAX_INDEX       = ITEMS - 1,
				ITEM_MASK       = (1ULL << ITEM_WIDTH) - 1,
			};

			/* analogous to 'Mmio::Register::Register_base' */
			typedef Register_array<OFFSET, ACCESS_WIDTH, ITEMS,
			                       ITEM_WIDTH, STRICT_WRITE>
				Register_array_base;

			typedef typename Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE>::
			                 access_t access_t;

			/**
			 * A bitregion within a register array item
			 *
			 * \param _SHIFT  bit shift of the first bit within an item
			 * \param _WIDTH  bit width of the region
			 *
			 * For details see 'Genode::Register::Bitfield'.
			 */
			template <unsigned long _SHIFT, unsigned long _SIZE>
			struct Bitfield :
				public Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE>::
				       template Bitfield<_SHIFT, _SIZE>
			{
				/* analogous to 'Mmio::Register::Register_base' */
				typedef Bitfield<_SHIFT, _SIZE> Array_bitfield_base;

				/* back reference to containing register array */
				typedef Register_array<OFFSET, ACCESS_WIDTH, ITEMS,
				                       ITEM_WIDTH, STRICT_WRITE>
					Compound_array;
			};

			/**
			 * Calculate destination of an array-item access
			 *
			 * \param offset  Gets overridden with the offset of the
			 *                access type instance, that contains the
			 *                access destination, relative to the MMIO
			 *                base.
			 * \param shift   Gets overridden with the shift of the
			 *                destination within the access type instance
			 *                targeted by 'offset'.
			 * \param index   index of the targeted array item
			 */
			static inline void dst(off_t & offset,
			                       unsigned long & shift,
			                       unsigned long const index)
			{
				unsigned long const bit_off = index << ITEM_WIDTH_LOG2;
				offset  = (off_t) ((bit_off >> BYTE_WIDTH_LOG2)
				          & ~(sizeof(access_t)-1) );
				shift   = bit_off - ( offset << BYTE_WIDTH_LOG2 );
				offset += OFFSET;
			}

			/**
			 * Calc destination of a simple array-item access without shift
			 *
			 * \param offset  gets overridden with the offset of the the
			 *                access destination, relative to the MMIO base
			 * \param index   index of the targeted array item
			 */
			static inline void simple_dst(off_t & offset,
			                              unsigned long const index)
			{
				offset  = (index << ITEM_WIDTH_LOG2) >> BYTE_WIDTH_LOG2;
				offset += OFFSET;
			}
		};

		addr_t const base; /* base address of targeted MMIO region */

		/**
		 * Constructor
		 *
		 * \param mmio_base  base address of targeted MMIO region
		 */
		inline Mmio(addr_t mmio_base) : base(mmio_base) { }


		/*************************
		 ** Access to registers **
		 *************************/

		/**
		 * Get the address of the register 'T' typed as its access type
		 */
		template <typename T>
		inline typename T::Register_base::access_t volatile * typed_addr() const
		{
			typedef typename T::Register_base Register;
			typedef typename Register::access_t access_t;
			return (access_t volatile *)(base + Register::OFFSET);
		}

		/**
		 * Read the register 'T'
		 */
		template <typename T>
		inline typename T::Register_base::access_t read() const
		{
			typedef typename T::Register_base Register;
			typedef typename Register::access_t access_t;
			return _read<access_t>(Register::OFFSET);
		}

		/**
		 * Override the register 'T'
		 */
		template <typename T>
		inline void
		write(typename T::Register_base::access_t const value)
		{
			typedef typename T::Register_base Register;
			typedef typename Register::access_t access_t;
			_write<access_t>(Register::OFFSET, value);
		}

		/******************************************
		 ** Access to bitfields within registers **
		 ******************************************/

		/**
		 * Read the bitfield 'T' of a register
		 */
		template <typename T>
		inline typename T::Bitfield_base::Compound_reg::access_t
		read() const
		{
			typedef typename T::Bitfield_base Bitfield;
			typedef typename Bitfield::Compound_reg Register;
			typedef typename Register::access_t access_t;
			return Bitfield::get(_read<access_t>(Register::OFFSET));
		}

		/**
		 * Override to the bitfield 'T' of a register
		 *
		 * \param value  value that shall be written
		 */
		template <typename T>
		inline void
		write(typename T::Bitfield_base::Compound_reg::access_t const value)
		{
			typedef typename T::Bitfield_base Bitfield;
			typedef typename Bitfield::Compound_reg Register;
			typedef typename Register::access_t access_t;

			/* initialize the pattern written finally to the register */
			access_t write_value;
			if (Register::STRICT_WRITE)
			{
				/* apply the bitfield to an empty write pattern */
				write_value = 0;
			} else {

				/* apply the bitfield to the old register value */
				write_value = read<Register>();
				Bitfield::clear(write_value);
			}
			/* apply bitfield value and override register */
			Bitfield::set(write_value, value);
			write<Register>(write_value);
		}


		/*******************************
		 ** Access to register arrays **
		 *******************************/

		/**
		 * Read an item of the register array 'T'
		 *
		 * \param index  index of the targeted item
		 */
		template <typename T>
		inline typename T::Register_array_base::access_t
		read(unsigned long const index) const
		{
			typedef typename T::Register_array_base Array;
			typedef typename Array::access_t access_t;

			/* reads outside the array return 0 */
			if (index > Array::MAX_INDEX) return 0;

			/* if item width equals access width we optimize the access */
			off_t offset;
			if (Array::ITEM_WIDTH == Array::ACCESS_WIDTH) {
				Array::simple_dst(offset, index);
				return _read<access_t>(offset);

			/* access width and item width differ */
			} else {
				long unsigned shift;
				Array::dst(offset, shift, index);
				return (_read<access_t>(offset) >> shift) &
				       Array::ITEM_MASK;
			}
		}

		/**
		 * Override an item of the register array 'T'
		 *
		 * \param value  value that shall be written
		 * \param index  index of the targeted item
		 */
		template <typename T>
		inline void
		write(typename T::Register_array_base::access_t const value,
		      unsigned long const index)
		{
			typedef typename T::Register_array_base Array;
			typedef typename Array::access_t access_t;

			/* ignore writes outside the array */
			if (index > Array::MAX_INDEX) return;

			/* optimize the access if item width equals access width */
			off_t offset;
			if (Array::ITEM_WIDTH == Array::ACCESS_WIDTH) {
				Array::simple_dst(offset, index);
				_write<access_t>(offset, value);

			/* access width and item width differ */
			} else {
				long unsigned shift;
				Array::dst(offset, shift, index);

				/* insert new value into old register value */
				access_t write_value;
				if (Array::STRICT_WRITE)
				{
					/* apply bitfield to an empty write pattern */
					write_value = 0;
				} else {

					/* apply bitfield to the old register value */
					write_value = _read<access_t>(offset);
					write_value &= ~(Array::ITEM_MASK << shift);
				}
				/* apply bitfield value and override register */
				write_value |= (value & Array::ITEM_MASK) << shift;
				_write<access_t>(offset, write_value);
			}
		}


		/*****************************************************
		 ** Access to bitfields within register array items **
		 *****************************************************/

		/**
		 * Read the bitfield 'T' of a register array
		 *
		 * \param index  index of the targeted item
		 */
		template <typename T>
		inline typename T::Array_bitfield_base::Compound_array::access_t
		read(unsigned long const index) const
		{
			typedef typename T::Array_bitfield_base Bitfield;
			typedef typename Bitfield::Compound_array Array;
			return Bitfield::get(read<Array>(index));
		}

		/**
		 * Override the bitfield 'T' of a register array
		 *
		 * \param value  value that shall be written
		 * \param index  index of the targeted array item
		 */
		template <typename T>
		inline void
		write(typename T::Array_bitfield_base::Compound_array::access_t const value,
		      long unsigned const index)
		{
			typedef typename T::Array_bitfield_base Bitfield;
			typedef typename Bitfield::Compound_array Array;
			typedef typename Array::access_t access_t;

			/* initialize the pattern written finally to the register */
			access_t write_value;
			if (Array::STRICT_WRITE)
			{
				/* apply the bitfield to an empty write pattern */
				write_value = 0;
			} else {

				/* apply the bitfield to the old register value */
				write_value = read<Array>(index);
				Bitfield::clear(write_value);
			}
			/* apply bitfield value and override register */
			Bitfield::set(write_value, value);
			write<Array>(write_value, index);
		}


		/***********************
		 ** Access to bitsets **
		 ***********************/

		/**
		 * Read bitset 'T' (composed of 2 parts)
		 */
		template <typename T>
		inline typename T::Bitset_2_base::access_t const read()
		{
			typedef typename T::Bitset_2_base::Bits_0 Bits_0;
			typedef typename T::Bitset_2_base::Bits_1 Bits_1;
			typedef typename T::Bitset_2_base::access_t access_t;
			enum { V1_SHIFT = Bits_0::BITFIELD_WIDTH };
			access_t const v0 = read<Bits_0>();
			access_t const v1 = read<Bits_1>();
			return v0 | (v1 << V1_SHIFT);
		}

		/**
		 * Override bitset 'T' (composed of 2 parts)
		 *
		 * \param v  value that shall be written
		 */
		template <typename T>
		inline void write(typename T::Bitset_2_base::access_t v)
		{
			typedef typename T::Bitset_2_base::Bits_0 Bits_0;
			typedef typename T::Bitset_2_base::Bits_1 Bits_1;
			write<Bits_0>(v);
			write<Bits_1>(v >> Bits_0::BITFIELD_WIDTH);
		}

		/**
		 * Read bitset 'T' (composed of 3 parts)
		 */
		template <typename T>
		inline typename T::Bitset_3_base::access_t const read()
		{
			typedef typename T::Bitset_3_base::Bits_0 Bits_0;
			typedef typename T::Bitset_3_base::Bits_1 Bits_1;
			typedef typename T::Bitset_3_base::Bits_2 Bits_2;
			typedef typename T::Bitset_3_base::access_t access_t;
			enum {
				BITS_0_WIDTH = Bits_0::BITFIELD_WIDTH,
				BITS_1_WIDTH = Bits_1::BITFIELD_WIDTH,
				V1_SHIFT     = BITS_0_WIDTH + BITS_1_WIDTH,
			};
			access_t const v0 = read<Bitset_2<Bits_0, Bits_1> >();
			access_t const v1 = read<Bits_2>();
			return v0 | (v1 << V1_SHIFT);
		}

		/**
		 * Override bitset 'T' (composed of 3 parts)
		 *
		 * \param v  value that shall be written
		 */
		template <typename T>
		inline void write(typename T::Bitset_3_base::access_t v)
		{
			typedef typename T::Bitset_3_base::Bits_0 Bits_0;
			typedef typename T::Bitset_3_base::Bits_1 Bits_1;
			typedef typename T::Bitset_3_base::Bits_2 Bits_2;
			write<Bitset_2<Bits_0, Bits_1> >(v);
			write<Bits_2>(v >> (Bits_0::BITFIELD_WIDTH +
			                    Bits_1::BITFIELD_WIDTH));
		}


		/*********************************
		 ** Polling for bitfield states **
		 *********************************/

		/**
		 * Interface for delaying the execution of a calling thread
		 */
		struct Delayer
		{
			/**
			 * Delay execution of the caller for 'us' microseconds
			 */
			virtual void usleep(unsigned us) = 0;
		};

		/**
		 * Wait until register 'T' contains the specified 'value'
		 *
		 * \param value         value to wait for
		 * \param delayer       sleeping facility to be used when the
		 *                      value is not reached yet
		 * \param max_attempts  number of register probing attempts
		 * \param us            number of microseconds between attempts
		 */
		template <typename T>
		inline bool
		wait_for(typename T::Register_base::access_t const value,
		         Delayer & delayer,
		         unsigned max_attempts = 500,
		         unsigned us           = 1000)
		{
			typedef typename T::Register_base Register;
			for (unsigned i = 0; i < max_attempts; i++, delayer.usleep(us))
			{
				if (read<Register>() == value) return true;
			}
			return false;
		}

		/**
		 * Wait until bitfield 'T' contains the specified 'value'
		 *
		 * \param value         value to wait for
		 * \param delayer       sleeping facility to be used when the
		 *                      value is not reached yet
		 * \param max_attempts  number of bitfield probing attempts
		 * \param us            number of microseconds between attempts
		 */
		template <typename T>
		inline bool
		wait_for(typename T::Bitfield_base::Compound_reg::access_t const value,
		         Delayer & delayer,
		         unsigned max_attempts = 500,
		         unsigned us           = 1000)
		{
			typedef typename T::Bitfield_base Bitfield;
			for (unsigned i = 0; i < max_attempts; i++, delayer.usleep(us))
			{
				if (read<Bitfield>() == value) return true;
			}
			return false;
		}
};

#endif /* _INCLUDE__UTIL__MMIO_H_ */
