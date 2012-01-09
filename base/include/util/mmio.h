/*
 * \brief  Generic MMIO accessor framework
 * \author Martin stein
 * \date   2011-10-26
 */

#ifndef _BASE__INCLUDE__UTIL__MMIO_H_
#define _BASE__INCLUDE__UTIL__MMIO_H_

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
			 * Write 'value' typed to MMIO base + 'o'
			 */
			template <typename STORAGE_T>
			void _write(off_t const o, STORAGE_T const value);

			/**
			 * Read typed from MMIO base + 'o'
			 */
			template <typename STORAGE_T>
			STORAGE_T _read(off_t const o) const;

		public:

			enum { BYTE_EXP = 3, BYTE_WIDTH = 8 };

			/**
			 * Holds additional info for an array of the inheriting type
			 */
			template <unsigned long SIZE>
			struct Array { enum { MAX_INDEX = SIZE-1 }; };

			/**
			 * A POD-like region at offset 'MMIO_OFFSET' within a MMIO region
			 */
			template <off_t MMIO_OFFSET, typename STORAGE_T>
			struct Register : public Genode::Register<STORAGE_T>
			{
				enum { OFFSET = MMIO_OFFSET };

				/**
				 * A bitregion within a register
				 */
				template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE>
				struct Subreg : public Genode::Register<STORAGE_T>::template Subreg<BIT_SHIFT, BIT_SIZE>
				{
					/**
					 * Back reference to containing register
					 */
					typedef Register<OFFSET, STORAGE_T> Compound_reg;
				};

				/**
				 * An array of 'SUBREGS' many similar bitregions with distance 'BIT_SHIFT'
				 * FIXME: Side effects of a combination of 'Reg_array' and 'Subreg_array'
				 *        are not evaluated
				 */
				template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE, unsigned long SUBREGS>
				struct Subreg_array : public Array<SUBREGS>
				{
					enum {
						SHIFT           = BIT_SHIFT,
						WIDTH           = BIT_SIZE,
						MASK            = (1 << WIDTH) - 1,
						ITERATION_WIDTH = (SHIFT + WIDTH),
						STORAGE_WIDTH   = BYTE_WIDTH * sizeof(STORAGE_T),
						ARRAY_SIZE      = (ITERATION_WIDTH * SUBREGS) >> BYTE_EXP,
					};

					/**
					 * Back reference to containing register
					 */
					typedef Register<OFFSET, STORAGE_T> Compound_reg;

					/**
					 * Calculate the MMIO-relative offset 'offset' and shift 'shift'
					 * within the according 'storage_t' value to acces subreg no. 'index'
					 */
					inline static void access_dest(off_t & offset, unsigned long & shift,
					                               unsigned long const index);
				};
			};

			/**
			 * An array of 'REGS' many similar 'Register's
			 */
			template <off_t MMIO_OFFSET, typename STORAGE_T, unsigned long REGS>
			struct Reg_array : public Register<MMIO_OFFSET, STORAGE_T>,
			                   public Array<REGS>
			{ };

			addr_t const base;

			/**
			 * Constructor
			 */
			inline Mmio(addr_t mmio_base);

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
			 * Read the subreg 'SUBREG'
			 */
			template <typename SUBREG>
			inline typename SUBREG::Compound_reg::storage_t read() const;

			/**
			 * Read the whole register no. 'index' of the array 'REG_ARRAY'
			 */
			template <typename REG_ARRAY>
			inline typename REG_ARRAY::storage_t read(unsigned long const index) const;

			/**
			 * Read the subreg no. 'index' of the array 'SUBREG_ARRAY'
			 */
			template <typename SUBREG_ARRAY>
			inline typename SUBREG_ARRAY::Compound_reg::storage_t read(unsigned long const index);

			/**
			 * Write 'value' into the register 'REGISTER'
			 */
			template <typename REGISTER>
			inline void write(typename REGISTER::storage_t const value);

			/**
			 * Write 'value' into the register no. 'index' of the array 'REG_ARRAY'
			 */
			template <typename REG_ARRAY>
			inline void write(typename REG_ARRAY::storage_t const value,
			                  unsigned long const index);

			/**
			 * Write 'value' into the subregister 'SUBREG'
			 */
			template <typename SUBREG>
			inline void write(typename SUBREG::Compound_reg::storage_t const value);

			/**
			 * Write 'value' into the bitfield no. 'index' of the array 'SUBREG_ARRAY'
			 */
			template <typename SUBREG_ARRAY>
			inline void write(typename SUBREG_ARRAY::Compound_reg::storage_t const value,
			                  unsigned long const index);
	};
}


template <Genode::off_t OFFSET, typename STORAGE_T>
template <unsigned long BIT_SHIFT, unsigned long BIT_SIZE, unsigned long SUBREGS>
void
Genode::Mmio::Register<OFFSET, STORAGE_T>::Subreg_array<BIT_SHIFT, BIT_SIZE, SUBREGS>::access_dest(Genode::off_t & offset,
                                                                                                   unsigned long & shift,
                                                                                                   unsigned long const index)
{
	unsigned long const bit_off = (index+1) * ITERATION_WIDTH - WIDTH;
	offset  = (off_t) ((bit_off / STORAGE_WIDTH) * sizeof(STORAGE_T));
	shift   = bit_off - ( offset << BYTE_EXP );
	offset += Compound_reg::OFFSET;
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


Genode::Mmio::Mmio(addr_t mmio_base) : base(mmio_base) { }


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


template <typename SUBREG>
typename SUBREG::Compound_reg::storage_t Genode::Mmio::read() const
{
	typedef typename SUBREG::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	return (_read<storage_t>(Register::OFFSET) >> SUBREG::SHIFT) & SUBREG::MASK;
}


template <typename REG_ARRAY>
typename REG_ARRAY::storage_t Genode::Mmio::read(unsigned long const index) const
{
	typedef typename REG_ARRAY::storage_t storage_t;

	if (index > REG_ARRAY::MAX_INDEX) return 0;

	addr_t const offset = REG_ARRAY::OFFSET + index*sizeof(storage_t);
	return _read<storage_t>(offset);
}


template <typename SUBREG_ARRAY>
typename SUBREG_ARRAY::Compound_reg::storage_t Genode::Mmio::read(unsigned long const index)
{
	enum { MASK = SUBREG_ARRAY::MASK };

	typedef typename SUBREG_ARRAY::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	if (index > SUBREG_ARRAY::MAX_INDEX) return;

	off_t const offset        = Register::OFFSET + SUBREG_ARRAY::access_off(index);
	unsigned long const shift = SUBREG_ARRAY::access_shift(index);

	return (_read<storage_t>(offset) >> shift) & MASK;
}


template <typename REGISTER>
void Genode::Mmio::write(typename REGISTER::storage_t const value)
{
	_write<typename REGISTER::storage_t>(REGISTER::OFFSET, value);
}


template <typename REG_ARRAY>
void Genode::Mmio::write(typename REG_ARRAY::storage_t const value,
                         unsigned long const index)
{
	typedef typename REG_ARRAY::storage_t storage_t;

	if (index > REG_ARRAY::MAX_INDEX) return;

	addr_t const offset = REG_ARRAY::OFFSET + index*sizeof(storage_t);
	_write<storage_t>(offset, value);
}


template <typename SUBREG>
void Genode::Mmio::write(typename SUBREG::Compound_reg::storage_t const value)
{
	typedef typename SUBREG::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	storage_t new_reg = read<Register>();

	new_reg &= ~(SUBREG::MASK << SUBREG::SHIFT);
	new_reg |= (value & SUBREG::MASK) << SUBREG::SHIFT;

	write<Register>(new_reg);
}


template <typename SUBREG_ARRAY>
void Genode::Mmio::write(typename SUBREG_ARRAY::Compound_reg::storage_t const value,
                         unsigned long const index)
{
	enum { MASK = SUBREG_ARRAY::MASK };

	typedef typename SUBREG_ARRAY::Compound_reg Register;
	typedef typename Register::storage_t storage_t;

	if (index > SUBREG_ARRAY::MAX_INDEX) return;

	off_t offset;
	unsigned long shift;
	SUBREG_ARRAY::access_dest(offset, shift, index);

	storage_t new_field = _read<storage_t>(offset);
	new_field &= ~(MASK << shift);
	new_field |= (value & MASK) << shift;

	_write<storage_t>(offset, new_field);
}


#endif /* _BASE__INCLUDE__UTIL__MMIO_H_ */

