/*
 * \brief  VMM dedicated hardware device passed-through to VM
 * \author Stefan Kalkowski
 * \date   2019-09-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__HW_DEVICE_H_
#define _SRC__SERVER__VMM__HW_DEVICE_H_

#include <exception.h>
#include <mmio.h>
#include <gic.h>

#include <base/attached_io_mem_dataspace.h>
#include <irq_session/connection.h>

namespace Vmm {
	template <unsigned, unsigned> class Hw_device;
}


template <unsigned MMIO_COUNT, unsigned IRQ_COUNT>
class Vmm::Hw_device
{
	private:

		class Irq : Gic::Irq::Irq_handler
		{
			private:

				using Session = Genode::Constructible<Genode::Irq_connection>;

				Gic::Irq               & _irq;
				Genode::Env            & _env;
				Session                  _session;
				Cpu::Signal_handler<Irq> _handler;

				void _assert()
				{
					_irq.assert();
				}

			public:

				void enabled() override
				{
					if (!_session.constructed()) {
						_session.construct(_env, _irq.number(),
						                   _irq.level() ?
						                   Genode::Irq_session::TRIGGER_LEVEL :
						                   Genode::Irq_session::TRIGGER_EDGE);
						_session->sigh(_handler);
						_session->ack_irq();
					}
				}

				void disabled() override
				{
					if (_session.constructed())
						_session.destruct();
				}

				void eoi() override
				{
					if (_session.constructed()) _session->ack_irq();
				}

				Irq(Gic::Irq & irq, Cpu & cpu, Genode::Env & env)
				: _irq(irq),
				  _env(env),
				  _handler(cpu, env.ep(), *this, &Irq::_assert)
				{
					_irq.handler(*this);
				}
		};

		Genode::Env           & _env;
		Genode::Vm_connection & _vm;
		Cpu                   & _cpu;

		Genode::Constructible<Genode::Attached_io_mem_dataspace> _ds[MMIO_COUNT];
		Genode::Constructible<Irq> _irqs[IRQ_COUNT];

		void mmio() { }
		void irqs() { }

	public:

		Hw_device(Genode::Env &env,
		          Genode::Vm_connection &vm,
		          Cpu & cpu)
		: _env(env), _vm(vm), _cpu(cpu) { };

		template <typename... ARGS>
		void mmio(Genode::addr_t start, Genode::size_t size, ARGS &&... args)
		{
			mmio(args...);
			for (unsigned i = 0; i < MMIO_COUNT; i++) {
				if (_ds[i].constructed()) continue;
				_ds[i].construct(_env, start, size);
				_vm.attach(_ds[i]->cap(), start);
				return;
			}
		}

		template <typename... ARGS>
		void irqs(unsigned irq, ARGS &&... args)
		{
			irqs(args...);
			for (unsigned i = 0; i < IRQ_COUNT; i++) {
				if (_irqs[i].constructed()) continue;
				_irqs[i].construct(_cpu.gic().irq(irq), _cpu, _env);
				return;
			}
		}
};

#endif /* _SRC__SERVER__VMM__HW_DEVICE_H_ */
