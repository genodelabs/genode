/*
 * \brief  Nova-specific instance of the IRQ object
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IRQ_OBJECT_H_
#define _CORE__INCLUDE__IRQ_OBJECT_H_

#include <nova/syscall-generic.h> /* Gsi_flags */

namespace Genode { class Irq_object; class Irq_args; }

class Genode::Irq_object
{
	private:

		Signal_context_capability _sigh_cap { };

		addr_t _kernel_caps;
		addr_t _msi_addr;
		addr_t _msi_data;
		addr_t _device_phys = 0; /* PCI config extended address */

		Nova::Gsi_flags _gsi_flags { };

		enum { KERNEL_CAP_COUNT_LOG2 = 0 };

		addr_t irq_sel() const { return _kernel_caps; }

	public:

		Irq_object();
		~Irq_object();

		addr_t msi_address() const { return _msi_addr; }
		addr_t msi_value()   const { return _msi_data; }

		void sigh(Signal_context_capability cap);
		void ack_irq();

		void start(unsigned irq, addr_t, Irq_args const &);
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
