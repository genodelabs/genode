/*
 * \brief  Generic accessor framework for highly structured memory regions
 * \author Martin stein
 * \date   2011-11-10
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__REGISTER_H_
#define _INCLUDE__UTIL__REGISTER_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode {

	template <unsigned long>                struct Register;
	template <typename, typename>           struct Bitset_2;
	template <typename, typename, typename> struct Bitset_3;
}


namespace Genode { namespace Trait {

		/**
		 * Round bit width up to an appropriate uint width or 0 if not feasible
		 */
		template <unsigned _WIDTH>
		struct Raise_to_uint_width
		{
			enum { WIDTH = _WIDTH < 9  ? 8  :
			               _WIDTH < 17 ? 16 :
			               _WIDTH < 33 ? 32 :
			               _WIDTH < 65 ? 64 : 0, };
		};

		/**
		 * Properties of integer types with a given bitwidth
		 */
		template <unsigned long _WIDTH> struct Uint_width;

		template <> struct Uint_width<8>
		{
			typedef uint8_t Type;
			enum { WIDTH_LOG2 = 3 };

			/**
			 * Access widths wich are dividers to the compound type width
			 */
			template <unsigned long _DIVISOR_WIDTH> struct Divisor;
		};

		template <> struct Uint_width<16> : Uint_width<8>
		{
			typedef uint16_t Type;
			enum { WIDTH_LOG2 = 4 };
		};

		template <> struct Uint_width<32> : Uint_width<16>
		{
			typedef uint32_t Type;
			enum { WIDTH_LOG2 = 5 };
		};

		template <> struct Uint_width<64> : Uint_width<32>
		{
			typedef uint64_t Type;
			enum { WIDTH_LOG2 = 6 };
		};

		template <> struct Uint_width<8>::Divisor<1>   { enum { WIDTH_LOG2 = 0 }; };
		template <> struct Uint_width<8>::Divisor<2>   { enum { WIDTH_LOG2 = 1 }; };
		template <> struct Uint_width<8>::Divisor<4>   { enum { WIDTH_LOG2 = 2 }; };
		template <> struct Uint_width<8>::Divisor<8>   { enum { WIDTH_LOG2 = 3 }; };
		template <> struct Uint_width<16>::Divisor<16> { enum { WIDTH_LOG2 = 4 }; };
		template <> struct Uint_width<32>::Divisor<32> { enum { WIDTH_LOG2 = 5 }; };
		template <> struct Uint_width<64>::Divisor<64> { enum { WIDTH_LOG2 = 6 }; };

		/**
		 * Reference 'Uint_width' through typenames
		 */
		template <typename _TYPE> struct Uint_type;

		template <> struct Uint_type<uint8_t>  : Uint_width<8> { };
		template <> struct Uint_type<uint16_t> : Uint_width<16> { };
		template <> struct Uint_type<uint32_t> : Uint_width<32> { };
		template <> struct Uint_type<uint64_t> : Uint_width<64> { };
	}
}


/**
 * An integer like highly structured memory region
 *
 * \param  _ACCESS_WIDTH  bit width of the region
 *
 * The register can contain multiple bitfields. Bitfields that are
 * partially exceed the register range are read and written also partially.
 * Bitfields that are completely out of the register range are read as '0'
 * and trying to overwrite them has no effect.
 */
template <unsigned long _ACCESS_WIDTH>
struct Genode::Register
{
	static constexpr size_t ACCESS_WIDTH      = _ACCESS_WIDTH;
	static constexpr size_t ACCESS_WIDTH_LOG2 = Trait::Uint_width<ACCESS_WIDTH>::WIDTH_LOG2;
	static constexpr size_t BITFIELD_WIDTH    = ACCESS_WIDTH;

	typedef typename Trait::Uint_width<ACCESS_WIDTH>::Type access_t;

	/**
	 * A bitregion within a register
	 *
	 * \param  _SHIFT  bit shift of first bit within the compound register
	 * \param  _WIDTH  bit width of the region
	 *
	 * Bitfields are read and written according to their range,
	 * so if we have a 'Bitfield<2,3>' and write '0b11101' to it
	 * only '0b101' (shiftet by 2 bits) is written.
	 */
	template <unsigned long _SHIFT, unsigned long _WIDTH>
	struct Bitfield
	{
		/**
		 * Fetch template parameters
		 */
		static constexpr size_t SHIFT          = _SHIFT;
		static constexpr size_t WIDTH          = _WIDTH;
		static constexpr size_t BITFIELD_WIDTH = WIDTH;

		typedef typename
			Trait::Uint_width<Trait::Raise_to_uint_width<WIDTH>::WIDTH>::Type
			bitfield_t;

		/**
		 * Get an unshifted mask of this field
		 */
		static constexpr access_t mask()
		{
			/* prevent compile error "left shift count >= width of type" */
			if (_WIDTH < _ACCESS_WIDTH) {
				return ((access_t)1 << _WIDTH) - 1;
			} else {
				return ~(access_t)0;
			}
		}

		/**
		 * Get a mask of this field shifted by its shift in the register
		 */
		static constexpr access_t reg_mask() { return (access_t)(mask() << SHIFT); }

		/**
		 * Get the bitwise negation of 'reg_mask'
		 */
		static constexpr access_t clear_mask() { return (access_t)~reg_mask(); }

		/**
		 * Back reference to containing register
		 */
		typedef Register<ACCESS_WIDTH> Compound_reg;

		/**
		 * Get register with this bitfield set to 'value' and rest left 0
		 *
		 * Useful to combine successive access to multiple
		 * bitfields into one operation.
		 */
		static inline access_t bits(access_t const value) {
			return access_t((value & mask()) << SHIFT); }

		/**
		 * Get a register value 'reg' masked according to this bitfield
		 *
		 * E.g. '0x1234' masked according to a
		 * 'Register<16>::Bitfield<5,7>' returns '0x0220'.
		 */
		static inline access_t masked(access_t const reg)
		{ return reg & reg_mask(); }

		/**
		 * Get value of this bitfield from 'reg'
		 */
		static inline bitfield_t get(access_t const reg)
		{ return (bitfield_t)((reg >> SHIFT) & mask()); }

		/**
		 * Get register value 'reg' with this bitfield set to zero
		 */
		static inline void clear(access_t & reg) { reg = reg & clear_mask(); }

		/**
		 * Get register value 'reg' with this bitfield set to 'value'
		 */
		static inline void set(access_t & reg, access_t const value = ~0)
		{
			clear(reg);
			reg = reg | (access_t)((value & mask()) << SHIFT);
		};
	};
};


/**
 * Bitfield that is composed of 2 separate parts
 *
 * \param _BITS_X  Register, bitfield or/and bitset types the
 *                 bitset is composed of. The order of arguments
 *                 is also the order of bit significance starting
 *                 with the least.
 */
template <typename _BITS_0, typename _BITS_1>
struct Genode::Bitset_2
{
	typedef _BITS_0 Bits_0;
	typedef _BITS_1 Bits_1;

	static constexpr size_t WIDTH          = Bits_0::BITFIELD_WIDTH +
	                                         Bits_1::BITFIELD_WIDTH;
	static constexpr size_t BITFIELD_WIDTH = WIDTH;
	static constexpr size_t ACCESS_WIDTH   = Trait::Raise_to_uint_width<WIDTH>::WIDTH;

	typedef typename Trait::Uint_width<ACCESS_WIDTH>::Type access_t;
	typedef Bitset_2<Bits_0, Bits_1> Bitset_2_base;

	/**
	 * Convert bitset value to register representation
	 *
	 * \param T  access type of register
	 * \param v  bitset value
	 */
	template <typename T>
	static inline T bits(T const v)
	{
		return Bits_0::bits(v) | Bits_1::bits(v >> Bits_0::WIDTH);
	}

	/**
	 * Override bitset in a given register value
	 *
	 * \param T      access type of register
	 * \param reg    register value
	 * \param value  new bitset value
	 */
	template <typename T>
	static inline void set(T & reg, access_t const value)
	{
		Bits_0::clear(reg);
		Bits_1::clear(reg);
		Bits_0::set(reg, value);
		Bits_1::set(reg, value >> Bits_0::WIDTH);
	}

	/**
	 * Read bitset from a given register value
	 *
	 * \param T      access type of register
	 * \param reg    register value
	 *
	 * \return  bitset value
	 */
	template <typename T>
	static inline access_t get(T const reg)
	{
		return (access_t)
			(Bits_0::get(reg) | (Bits_1::get(reg) << Bits_0::WIDTH));
	}
};


/**
 * Bitfield that is composed of 3 separate parts
 *
 * \param _BITS_X  Register, bitfield or/and bitset types the
 *                 bitset is composed of. The order of arguments
 *                 is also the order of bit significance starting
 *                 with the least.
 */
template <typename _BITS_0, typename _BITS_1, typename _BITS_2>
struct Genode::Bitset_3
{
	typedef _BITS_0 Bits_0;
	typedef _BITS_1 Bits_1;
	typedef _BITS_2 Bits_2;
	typedef Bitset_2<Bits_0, Bits_1> Bits_0_1;

	static constexpr size_t WIDTH          = Bits_0::BITFIELD_WIDTH +
	                                         Bits_1::BITFIELD_WIDTH +
	                                         Bits_2::BITFIELD_WIDTH;
	static constexpr size_t BITFIELD_WIDTH = WIDTH;
	static constexpr size_t ACCESS_WIDTH   = Trait::Raise_to_uint_width<WIDTH>::WIDTH;

	typedef typename Trait::Uint_width<ACCESS_WIDTH>::Type access_t;
	typedef Bitset_3<Bits_0, Bits_1, Bits_2> Bitset_3_base;

	/**
	 * Convert bitset value to register representation
	 *
	 * \param T  access type of register
	 * \param v  bitset value
	 */
	template <typename T>
	static inline T bits(T const v)
	{
		return Bits_0_1::bits(v) | Bits_2::bits(v >> Bits_0_1::WIDTH);
	}

	/**
	 * Override bitset in a given register value
	 *
	 * \param T      access type of register
	 * \param reg    register value
	 * \param value  new bitset value
	 */
	template <typename T>
	static inline void set(T & reg, access_t const value)
	{
		Bits_0::clear(reg);
		Bits_1::clear(reg);
		Bits_2::clear(reg);
		Bits_0_1::set(reg, value);
		Bits_2::set(reg, value >> Bits_0_1::WIDTH);
	}

	/**
	 * Read bitset from a given register value
	 *
	 * \param T      access type of register
	 * \param reg    register value
	 *
	 * \return  bitset value
	 */
	template <typename T>
	static inline access_t get(T const reg)
	{
		return Bits_0_1::get(reg) | (Bits_2::get(reg) << Bits_0_1::WIDTH);
	}
};

#endif /* _INCLUDE__UTIL__REGISTER_H_ */
