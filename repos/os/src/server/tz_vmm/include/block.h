/*
 * \brief  Paravirtualized access to block devices for a Trustzone VM
 * \author Martin Stein
 * \date   2015-10-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TZ_VMM__INCLUDE__BLOCK_H_
#define _TZ_VMM__INCLUDE__BLOCK_H_

/* Genode includes */
#include <vm_state.h>

/* local includes */
#include <vm_base.h>

namespace Vmm { class Block; }

/**
 * Paravirtualized access to block devices for a Trustzone VM
 */
class Vmm::Block
{
	private:

		class Oversized_request : public Genode::Exception { };

		void *         _buf;
		Genode::size_t _buf_size;

		void _buf_to_pkt(void * const dst, Genode::size_t const sz);

		void _name(Vm_base * const vm);

		void _block_count(Vm_base * const vm);

		void _block_size(Vm_base * const vm);

		void _queue_size(Vm_base * const vm);

		void _writeable(Vm_base * const vm);

		void _irq(Vm_base * const vm);

		void _buffer(Vm_base * const vm);

		void _start_callback(Vm_base * const vm);

		void _device_count(Vm_base * const vm);

		void _new_request(Vm_base * const vm);

		void _submit_request(Vm_base * const vm);

		void _collect_reply(Vm_base * const vm);

	public:

		/**
		 * Handle Secure Monitor Call of VM 'vm' on VMM block
		 */
		void handle(Vm_base * const vm);

		Block() : _buf_size(0) { }
};

#endif /* _TZ_VMM__INCLUDE__BLOCK_H_ */
