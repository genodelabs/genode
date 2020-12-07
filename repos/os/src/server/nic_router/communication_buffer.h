/*
 * \brief  Buffer for network communication
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMMUNICATION_BUFFER_H_
#define _COMMUNICATION_BUFFER_H_

/* Genode includes */
#include <dataspace/capability.h>
#include <base/ram_allocator.h>

namespace Net { class Communication_buffer; }


class Net::Communication_buffer
{
	private:

		Genode::Ram_allocator            &_ram_alloc;
		Genode::Ram_dataspace_capability  _ram_ds;

	public:

		Communication_buffer(Genode::Ram_allocator &ram_alloc,
		                     Genode::size_t const   size);

		~Communication_buffer() { _ram_alloc.free(_ram_ds); }


		/***************
		 ** Accessors **
		 ***************/

		Genode::Dataspace_capability ds() const { return _ram_ds; }
};

#endif /* _COMMUNICATION_BUFFER_H_ */
