/*
 * \brief  Broadwell context descriptor
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONTEXT_DESCRIPTOR_H_
#define _CONTEXT_DESCRIPTOR_H_

/* Genode includes */
#include <util/register.h>

/* local includes */
#include <types.h>


namespace Igd {

	class Context_descriptor;
}


/*
 * IHD-OS-BDW-Vol 2d-11.15 p. 107
 */
class Igd::Context_descriptor : Genode::Register<64>
{
	public:

		/*
		 * Context ID covers 63:32 where we currently only
		 * care about the lowest 20 bits.
		 *
		 * 63:55  group ID
		 * 54     MBZ
		 * 53     MBZ
		 * 20:0   globally unique SW controlled ID
		 */
		struct Context_id                   : Bitfield<32, 20> { };
		struct Logical_ring_context_address : Bitfield<12, 20> { };
		struct Privilege_access             : Bitfield< 8,  1> { };
		struct Fault_handling               : Bitfield< 6,  2>
		{
			enum { FAULT_AND_HANG = 0b00, };
		};
		struct Addressing_mode              : Bitfield< 3,  2>
		{
			enum {
				ADVANCED_WO_AD   = 0b00,
				LEGACY_WO_64     = 0b01,
				ADVANCED_WITH_AD = 0b10,
				LEGACY_WITH_64   = 0b11,
			};
		};
		struct Force_restore                : Bitfield< 2,  1> { };
		struct Force_pd_restore             : Bitfield< 1,  1> { };
		struct Valid                        : Bitfield< 0,  1> { };

	private:

		uint64_t _value = 0;

	public:

		/**
		 * Constructor
		 *
		 * \param id    context id
		 * \param lrca  graphics memory address of the context
		 */
		Context_descriptor(uint32_t  id,
		                   addr_t    lrca)
		{
			/* shift lrca value accordingly */
			typename Context_descriptor::access_t const addr = Context_descriptor::Logical_ring_context_address::get(lrca);
			Logical_ring_context_address::set(_value, addr);

			Privilege_access::set(_value, 1);
			/* must be set to FAULT_AND_HANG according to PRM when legacy mode is used */
			Fault_handling::set(_value, Fault_handling::FAULT_AND_HANG);
			Addressing_mode::set(_value, Addressing_mode::LEGACY_WITH_64);
			Context_id::set(_value, id);
			Force_restore::set(_value, 1);
			Force_pd_restore::set(_value, 1);
			Valid::set(_value, 1);
		}

		/**
		 * Constructor
		 */
		Context_descriptor() : _value(0) { }

		uint32_t low()   const { return (uint32_t)(_value & 0xffffffff); }
		uint32_t high()  const { return (uint32_t)(_value >> 32); }
		bool     valid() const { return Valid::get(_value) == 1; }
		uint64_t value() const { return _value; }

		/*********************
		 ** Debug interface **
		 *********************/

		void dump()
		{
			using namespace Genode;

			log("Context_descriptor: ", Hex(_value, Hex::PREFIX, Hex::PAD));
		}
};

#endif /* _CONTEXT_DESCRIPTOR_H_ */
