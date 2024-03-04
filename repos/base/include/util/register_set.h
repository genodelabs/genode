/*
 * \brief  Set of fine-grained and typesafe accessible registers with offsets
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__REGISTER_SET_H_
#define _INCLUDE__UTIL__REGISTER_SET_H_

/* Genode includes */
#include <util/register.h>
#include <util/noncopyable.h>
#include <util/interface.h>
#include <base/exception.h>

namespace Genode {

	struct Register_set_plain_access;
	struct Register_set_base;
	template <typename, size_t> class Register_set;
}


/**
 * Interface declaration for implementations of plain access to a register set
 *
 * This class enables us to use the bit and offset logic of the Register_set
 * template with different plain access implementations and, at the same time,
 * keep the plain access implementation invisible at the front-end. The class
 * circumvents the fact that we can't have virtual method templates nor
 * template friends. The plain access implementation should keep its methods
 * private and declare this class as friend. It is then handed over as
 * argument to the Register_set template.
 */
struct Genode::Register_set_plain_access
{
	/**
	 * Write on plain integer level to a register of the set
	 *
	 * \param ACCESS_T        plain integer type of the register to access
	 * \param IMPLEMENTATION  implementation of access on the plain integer level
	 * \param impl            instance of the access implementation
	 * \param offset          register offset
	 * \param value           value to be written to the register
	 */
	template <typename ACCESS_T, typename IMPLEMENTATION>
	static inline
	void write(IMPLEMENTATION &impl, off_t const offset, ACCESS_T const value) {
		impl.template _write<ACCESS_T>(offset, value); }

	/**
	 * Read on plain integer level from a register of the set
	 *
	 * \param ACCESS_T        plain integer type of the register to access
	 * \param IMPLEMENTATION  implementation of access on the plain integer level
	 * \param impl            instance of the access implementation
	 * \param offset          register offset
	 */
	template <typename ACCESS_T, typename IMPLEMENTATION>
	static inline ACCESS_T read(IMPLEMENTATION &impl, off_t const offset) {
		return impl.template _read<ACCESS_T>(offset); }
};


struct Genode::Register_set_base : Noncopyable
{
	/**
	 * Interface for delaying the execution of a calling thread
	 */
	struct Delayer : Interface
	{
		/**
		 * Delay execution of the caller for 'us' microseconds
		 */
		virtual void usleep(uint64_t us) = 0;
	};
};


/**
 * Set of fine-grained and typesafe accessible registers with offsets
 *
 * \param PLAIN_ACCESS  Implementation of access on the plain integer level.
 *                      Should derive from the Register_set_plain_access
 *                      interface.
 *
 * A register set consists of individual registers and register arrays with
 * different offsets. The class receives the specific implementation of plain
 * access as argument. This way, it can be used with different types of IO
 * (MMIO, I2C, Port IO, ...) . For correct behavior of the 'Register_set'
 * methods, a class that derives from one of the subclasses of 'Register_set'
 * must not define members named 'Register_base', 'Bitfield_base',
 * 'Register_array_base' or 'Array_bitfield_base'.
 */
template <typename PLAIN_ACCESS, Genode::size_t REGISTER_SET_SIZE>
class Genode::Register_set : public Register_set_base
{
	private:

		using Plain_access = Register_set_plain_access;

		enum { BYTE_WIDTH_LOG2 = 3, BYTE_WIDTH = 1 << BYTE_WIDTH_LOG2 };

		/**
		 * Return wether one IO condition is met
		 *
		 * \param CONDITION  A condition subtype of Register, Bitfield, or
		 *                   such (for example the Bitfield::Equal type)
		 * \param condition  the condition instance
		 */
		template <typename CONDITION>
		inline bool _conditions_met(CONDITION condition) {
			return condition.met(read<typename CONDITION::Object>()); }

		template <typename CONDITION>
		inline bool _one_condition_met(CONDITION condition) {
			return condition.met(read<typename CONDITION::Object>()); }

		/**
		 * Return wether a list of IO conditions is met
		 *
		 * \param CONDITION   The type of the head of the condition list.
		 *                    A condition subtype of Register, Bitfield, or
		 *                    such (for example the Bitfield::Equal type).
		 * \param CONDITIONS  The types of the tail of the condition list.
		 *                    condition subtypes of Register, Bitfield, or
		 *                    such (for example the Bitfield::Equal type)
		 * \param head        the condition instance
		 */
		template <typename CONDITION, typename... CONDITIONS>
		inline bool _conditions_met(CONDITION head, CONDITIONS... tail) {
			return _conditions_met(head) ? _conditions_met(tail...) : false; }

		/**
		 * Same as '_conditions_met' but returns true if one condition in a list of
		 * IO conditions is met
		 */
		template <typename CONDITION, typename... CONDITIONS>
		inline bool _one_condition_met(CONDITION head, CONDITIONS... tail) {
			return _conditions_met(head) ? true : _one_condition_met(tail...); }

		/**
		 * This template equips registers/bitfields with conditions for polling
		 *
		 * \param T  register/bitfield type that the conditions shall be about
		 *
		 * The condition subtypes enable us to poll for a variable amount of
		 * conditions for different registers/bitfields at once. They are used
		 * as input to the 'wait_for' template.
		 */
		template <typename T>
		struct Conditions
		{
			/**
			 * Condition that the register/bitfield equals a value
			 */
			class Equal
			{
				private:

					typedef typename T::access_t access_t;

					access_t const _reference_val;

				public:

					typedef T Object;

					/**
					 * Constructor
					 *
					 * \param reference_val  reference value
					 */
					explicit Equal(access_t const reference_val)
					: _reference_val(reference_val) { }

					/**
					 * Return whether the condition is met
					 *
					 * \param actual_val  actual regster/bitfield value
					 */
					bool met(access_t const actual_val) const {
						return _reference_val == actual_val; }
			};
		};

		PLAIN_ACCESS &_plain_access;

	public:

		/**
		 * An integer like region at a specific place within a register set
		 *
		 * \param _OFFSET        Offset of the region in the register set
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
		struct Register
		:
			public Genode::Register<_ACCESS_WIDTH>,
			public Conditions<Register<_OFFSET, _ACCESS_WIDTH, _STRICT_WRITE> >
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

			typedef typename Genode::Register<_ACCESS_WIDTH>::access_t
				access_t;

			static_assert(OFFSET + sizeof(access_t) <= REGISTER_SET_SIZE);

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
			struct Bitfield
			:
				public Genode::Register<ACCESS_WIDTH>::
				               template Bitfield<_SHIFT, _WIDTH>,
				public Conditions<Bitfield<_SHIFT, _WIDTH> >
			{
				/* analogous to 'Register_set::Register::Register_base' */
				typedef Bitfield<_SHIFT, _WIDTH> Bitfield_base;

				/* back reference to containing register */
				typedef Register<_OFFSET, _ACCESS_WIDTH, _STRICT_WRITE>
					Compound_reg;

				typedef Compound_reg::access_t access_t;
			};
		};

		/**
		 * An array of successive equally structured regions, called items
		 *
		 * \param _OFFSET        Offset of the first item in the register set
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
				/* prevent shifting more than bit width by setting all bits explicitly */
				ITEM_MASK       = (ITEM_WIDTH == 64) ? ~0ULL : (1ULL << ITEM_WIDTH) - 1,
			};

			/* analogous to 'Register_set::Register::Register_base' */
			typedef Register_array<OFFSET, ACCESS_WIDTH, ITEMS,
			                       ITEM_WIDTH, STRICT_WRITE>
				Register_array_base;

			typedef typename Register<OFFSET, ACCESS_WIDTH, STRICT_WRITE>::
			                 access_t access_t;

			/**
			 * A bit region within a register array item
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
				/* analogous to 'Register_set::Register::Register_base' */
				typedef Bitfield<_SHIFT, _SIZE> Array_bitfield_base;

				/* back reference to containing register array */
				typedef Register_array<OFFSET, ACCESS_WIDTH, ITEMS,
				                       ITEM_WIDTH, STRICT_WRITE>
					Compound_array;
			};


			struct Dst { off_t offset; uint8_t shift; };

			/**
			 * Calculate destination of an array-item access
			 *
			 * \param index   index of the targeted array item
			 */
			static constexpr Dst dst(unsigned long index)
			{
				off_t   bit_offset  = off_t(index << ITEM_WIDTH_LOG2);
				off_t   byte_offset = bit_offset >> BYTE_WIDTH_LOG2;
				off_t   offset      = byte_offset & ~(sizeof(access_t) - 1);
				uint8_t shift       = uint8_t(bit_offset - (offset << BYTE_WIDTH_LOG2));
				offset += OFFSET;

				return { .offset = offset, .shift = shift };
			}

			static_assert(dst(MAX_INDEX).offset + sizeof(access_t) <= REGISTER_SET_SIZE);

			/**
			 * Calc destination of a simple array-item access without shift
			 *
			 * \param offset  gets overridden with the offset of the the
			 *                access destination
			 * \param index   index of the targeted array item
			 */
			static inline void simple_dst(off_t & offset,
			                              unsigned long const index)
			{
				offset  = (index << ITEM_WIDTH_LOG2) >> BYTE_WIDTH_LOG2;
				offset += OFFSET;
			}
		};

		/**
		 * Constructor
		 *
		 * \param plain_access  implementation of plain register access
		 */
		Register_set(PLAIN_ACCESS &plain_access)
		: _plain_access(plain_access) { }


		/*************************
		 ** Access to registers **
		 *************************/

		/**
		 * Read the register 'T'
		 */
		template <typename T>
		inline typename T::Register_base::access_t read() const
		{
			typedef typename T::Register_base Register;
			typedef typename Register::access_t access_t;
			return Plain_access::read<access_t>(_plain_access,
			                                    Register::OFFSET);
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
			Plain_access::write<access_t>(_plain_access, Register::OFFSET,
			                              value);
		}

		/******************************************
		 ** Access to bitfields within registers **
		 ******************************************/

		/**
		 * Read the bitfield 'T' of a register
		 */
		template <typename T>
		inline typename T::Bitfield_base::bitfield_t
		read() const
		{
			typedef typename T::Bitfield_base Bitfield;
			typedef typename Bitfield::Compound_reg Register;
			typedef typename Register::access_t access_t;
			return
				Bitfield::get(Plain_access::read<access_t>(_plain_access,
				                                           Register::OFFSET));
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
				return Plain_access::read<access_t>(_plain_access, offset);

			/* access width and item width differ */
			} else {
				typename Array::Dst dst { Array::dst(index) };
				return (Plain_access::read<access_t>(_plain_access, dst.offset)
				        >> dst.shift) & Array::ITEM_MASK;
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
				Plain_access::write<access_t>(_plain_access, offset, value);

			/* access width and item width differ */
			} else {
				typename Array::Dst dst { Array::dst(index) };

				/* insert new value into old register value */
				access_t write_value;
				if (Array::STRICT_WRITE)
				{
					/* apply bitfield to an empty write pattern */
					write_value = 0;
				} else {

					/* apply bitfield to the old register value */
					write_value = Plain_access::read<access_t>(_plain_access,
					                                           dst.offset);
					write_value &= ~(Array::ITEM_MASK << dst.shift);
				}
				/* apply bitfield value and override register */
				write_value |= (value & Array::ITEM_MASK) << dst.shift;
				Plain_access::write<access_t>(_plain_access, dst.offset,
				                              write_value);
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
		inline typename T::Array_bitfield_base::bitfield_t
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
			write<Bits_0>(typename Bits_0::access_t(v));
			write<Bits_1>(typename Bits_1::access_t(v >> Bits_0::BITFIELD_WIDTH));
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

			static constexpr size_t BITS_0_WIDTH = Bits_0::BITFIELD_WIDTH;
			static constexpr size_t BITS_1_WIDTH = Bits_1::BITFIELD_WIDTH;
			static constexpr size_t V1_SHIFT     = BITS_0_WIDTH + BITS_1_WIDTH;

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
			write<Bitset_2<Bits_0, Bits_1> >(typename Bitset_2<Bits_0, Bits_1>::access_t(v));
			write<Bits_2>(typename Bits_2::access_t(v >> (Bits_0::BITFIELD_WIDTH +
			                                              Bits_1::BITFIELD_WIDTH)));
		}


		/*********************************
		 ** Polling for bitfield states **
		 *********************************/

		struct Polling_timeout : Exception { };

		struct Attempts
		{
			unsigned value;
			explicit Attempts(unsigned value) : value(value) { }
		};

		struct Microseconds
		{
			uint64_t value;
			explicit Microseconds(uint64_t value) : value(value) { }
		};


		/**
		 * Wait until a list of IO conditions is met
		 *
		 * \param CONDITIONS    Types of the of conditions in the condition
		 *                      list. Condition subtypes of the IO types. For
		 *                      example the Bitfield::Equal type.
		 * \param attempts      maximum number of probing attempts
		 * \param us            number of microseconds between attempts
		 * \param delayer       Sleeping facility to be used when the
		 *                      conditions are not met
		 * \param conditions    condition list
		 *
		 * \throw Polling_timeout
		 */
		template <typename... CONDITIONS>
		inline void wait_for(Attempts       attempts,
		                     Microseconds   us,
		                     Delayer       &delayer,
		                     CONDITIONS...  conditions)
		{
			for (unsigned i = 0; i < attempts.value; i++,
			     delayer.usleep(us.value))
			{
				if (_conditions_met(conditions...)) {
					return; }
			}
			throw Polling_timeout();
		}

		/**
		 * Shortcut for 'wait_for' with 'attempts = 500' and 'us = 1000'
		 */
		template <typename... CONDITIONS>
		inline void wait_for(Delayer &delayer, CONDITIONS... conditions)
		{
			wait_for<CONDITIONS...>(Attempts(500), Microseconds(1000),
			                        delayer, conditions...);
		}

		/**
		 * Same as 'wait_for' but wait until one condition in a list of IO
		 * conditions is met
		 */
		template <typename... CONDITIONS>
		inline void wait_for_any(Attempts       attempts,
		                         Microseconds   us,
		                         Delayer       &delayer,
		                         CONDITIONS...  conditions)
		{
			for (unsigned i = 0; i < attempts.value; i++,
			     delayer.usleep(us.value))
			{
				if (_one_condition_met(conditions...)) {
					return; }
			}
			throw Polling_timeout();
		}

		/**
		 * Shortcut for 'wait_for' with 'attempts = 500' and 'us = 1000'
		 */
		template <typename... CONDITIONS>
		inline void wait_for_any(Delayer &delayer, CONDITIONS... conditions)
		{
			wait_for_any<CONDITIONS...>(Attempts(500), Microseconds(1000),
			                            delayer, conditions...);
		}
};

#endif /* _INCLUDE__UTIL__REGISTER_SET_H_ */
