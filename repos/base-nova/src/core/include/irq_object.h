/*
 * \brief  Nova-specific instance of the IRQ object
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__IRQ_OBJECT_H_
#define _CORE__INCLUDE__IRQ_OBJECT_H_

namespace Genode { class Irq_object; }

class Genode::Irq_object
{
	private:

		Signal_context_capability _sigh_cap;

		Genode::addr_t            _kernel_caps;
		Genode::addr_t            _msi_addr;
		Genode::addr_t            _msi_data;
		Genode::addr_t            _device_phys; /* PCI config extended address */

		enum { KERNEL_CAP_COUNT_LOG2 = 0 };

		Genode::addr_t const irq_sel() { return _kernel_caps; }

	public:

		Irq_object();
		~Irq_object();

		Genode::addr_t msi_address() const { return _msi_addr; }
		Genode::addr_t msi_value()   const { return _msi_data; }

		void sigh(Signal_context_capability cap);
		void ack_irq();

		void start(unsigned irq, Genode::addr_t);
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
