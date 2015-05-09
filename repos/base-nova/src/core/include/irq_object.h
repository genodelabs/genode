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

#pragma once

#include <base/thread.h>

namespace Genode { class Irq_object; }

class Genode::Irq_object : public Thread<4096> {

	private:

		Signal_context_capability _sig_cap;
		Lock                      _sync_ack;
		Lock                      _sync_life;

		Genode::addr_t            _kernel_caps;
		Genode::addr_t            _msi_addr;
		Genode::addr_t            _msi_data;
		enum { UNDEFINED, SHUTDOWN } volatile _state;

		void entry() override;

		enum { KERNEL_CAP_COUNT_LOG2 = 1 };

		Genode::addr_t const irq_sel() { return _kernel_caps; }
		Genode::addr_t const sc_sel()  { return _kernel_caps + 1; }

	public:

		Irq_object();
		~Irq_object();

		Genode::addr_t msi_address() const { return _msi_addr; }
		Genode::addr_t msi_value()   const { return _msi_data; }

		void sigh(Signal_context_capability cap) { _sig_cap = cap; }
		void notify() { Genode::Signal_transmitter(_sig_cap).submit(1); }

		void ack_irq() { _sync_ack.unlock(); }

		void start() override;
		void start(unsigned irq, Genode::addr_t);
};
