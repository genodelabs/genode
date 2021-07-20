/*
 * \brief  Allocator for hardware-specific address-space identifiers
 * \author Martin Stein
 * \date   2021-07-21
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ARM_V8__ADDRESS_SPACE_ID_ALLOCATOR_H_
#define _ARM_V8__ADDRESS_SPACE_ID_ALLOCATOR_H_

/* base includes */
#include <util/bit_allocator.h>

namespace Board {

	class Address_space_id_allocator : public Genode::Bit_allocator<65536> { };
}

#endif /* _ARM_V8__ADDRESS_SPACE_ID_ALLOCATOR_H_ */
