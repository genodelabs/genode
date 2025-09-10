/*
 * \brief  Core-specific instance of the IRQ session interface
 * \author Christian Helmuth
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__IRQ_OBJECT_H_
#define _CORE__INCLUDE__IRQ_OBJECT_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/thread.h>
#include <irq_session/irq_session.h>

/* base internal includes */
#include <base/internal/capability_space_sel4.h>

/* core includes */
#include <types.h>
#include <irq_args.h>
#include <cap_sel_alloc.h>

namespace Core { class Irq_object; }


class Core::Irq_object : public Thread {

	private:

		Signal_context_capability _sig_cap { };
		Blockade                  _sync_bootup { };
		Allocator::Result const  &_irq;
		volatile bool             _stop { };

		Cap_sel_alloc::Cap_sel_attempt _kernel_irq_sel;
		Cap_sel_alloc::Cap_sel_attempt _kernel_notify_sel;
		Allocator::Alloc_result        _kernel_notify_phys { };

		void _wait_for_irq();

		void entry() override;

		long _associate(Irq_args const &);

	public:

		/* see contrib/sel4/src/kernel/include/plat/pc99/plat/machine.h */
		enum {
			PIC_IRQ_LINES  = 16,
			IRQ_INT_OFFSET = 0x20,
			MSI_OFFSET     = PIC_IRQ_LINES + IRQ_INT_OFFSET
		};

		Irq_object(Runtime &, Allocator::Result const &irq);
		~Irq_object();

		void sigh(Signal_context_capability cap) { _sig_cap = cap; }
		void notify() { Signal_transmitter(_sig_cap).submit(1); }
		void ack_irq();

		Start_result start() override;
		bool associate(Irq_args const &);

		bool msi() const
		{
			return with_irq<bool>([&](auto const irq) {
				return irq >= MSI_OFFSET;
			}, false);
		}

		void stop_thread();

		template <typename T>
		T with_irq(auto const &fn, T const fail_value) const
		{
			return _irq.convert<T>([&](auto const &a) -> T {
				return fn(addr_t(a.ptr));
			}, [&](auto) -> T { return fail_value; });
		}
};

#endif /* _CORE__INCLUDE__IRQ_OBJECT_H_ */
