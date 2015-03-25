/*
 * \brief  Implementation of shared IRQs in PCI driver
 * \author Alexander Boettcher
 * \date   2015-03-27
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <irq_session/connection.h>

/* Genode OS includes */
#include <platform/irq_proxy.h>

/* PCI driver include */
#include "irq.h"


namespace Pci {
	class Irq_component;
	class Irq_allocator;
	class Irq_thread;
}


using namespace Genode;


/**
 * A simple range allocator implementation used by the Irq_proxy
 */
class Pci::Irq_allocator : public Range_allocator, Bit_allocator<256>
{
	Alloc_return alloc_addr(size_t size, addr_t addr) override
	{
		try {
			_array.set(addr, size);
			return Alloc_return::OK;
		} catch (...) {
			return Alloc_return::RANGE_CONFLICT;
		}
	}

	/* unused methods */
	int    remove_range(addr_t, size_t) override { return 0; }
	int    add_range(addr_t, size_t)    override { return 0; }
	bool   valid_addr(addr_t)     const override { return false; }
	size_t avail()                const override { return 0; }
	bool   alloc(size_t, void **)       override { return false; }
	void   free(void *)                 override { }
	void   free(void *, size_t)         override { }
	size_t overhead(size_t)       const override { return 0; }
	bool   need_size_for_free()   const override { return 0; }

	Alloc_return alloc_aligned(size_t, void **, int, addr_t, addr_t) override {
		 return Alloc_return::RANGE_CONFLICT; }
};


/**
 * Required by Irq_proxy if we would like to have a thread per IRQ,
 * which we don't want to in the PCI driver - one thread is sufficient.
 */
class NoThread
{
	public:

		NoThread(const char *) { }

		void start(void) { }
};


/**
 * Thread waiting for signals caused by IRQs 
 */
class Pci::Irq_thread : public Genode::Thread<4096>
{
	private:

		Signal_receiver _sig_rec;

	public:

		Irq_thread() : Thread<4096>("irq_sig_recv") { start(); }

		Signal_receiver & sig_rec() { return _sig_rec; }

		void entry() {

			typedef Genode::Signal_dispatcher_base Sdb;

			while (1) {
				Genode::Signal sig = _sig_rec.wait_for_signal();

				Sdb *dispatcher = dynamic_cast<Sdb *>(sig.context());

				if (!dispatcher) {
					PERR("dispatcher missing for signal %p, %u",
					     sig.context(), sig.num());
					continue;
				}
				dispatcher->dispatch(sig.num());
			}
		}
};


/**
 * One allocator for managing in use IRQ numbers and one IRQ thread waiting
 * for Genode signals of all hardware IRQs.
 */
static Pci::Irq_allocator irq_alloc;
static Pci::Irq_thread    irq_thread;


/**
 * Irq_proxy interface implementation
 */
typedef Genode::Irq_proxy<NoThread> Proxy;

class Pci::Irq_component : public Proxy
{
	private:

		Genode::Irq_connection _irq;
		Genode::Signal_dispatcher<Pci::Irq_component> _irq_dispatcher;

		bool _associated;

	public:

		void _ack_irq() {
			/*
			 * Associate handler only when required, because our partner may
			 * also implement shared irq and would expect to get ack_irq()
			 * form us even if we have no client ...
			 */
			if (!_associated) {
				_associated = true;
				/* register signal handler on irq_session */
				_irq.sigh(_irq_dispatcher);
			}

			_irq.ack_irq();
		}

		bool _associate()    { return _associated; }
		void _wait_for_irq() { }

		virtual bool remove_sharer(Irq_sigh *s) override {
			if (!Proxy::remove_sharer(s))
				return false;

			/* De-associate handler. */
			_associated = false;
			_irq.sigh(Genode::Signal_context_capability());
			return true;
		}

	public:

		Irq_component(unsigned gsi)
		:
			Proxy(gsi), _irq(gsi), _irq_dispatcher(irq_thread.sig_rec(), *this,
			                                       &::Proxy::notify_about_irq),
			_associated(false)
		{ }
};



/*******************************
 ** PCI IRQ session component **
 *******************************/

void Pci::Irq_session_component::ack_irq()
{
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _gsi);
		return;
	}

	if (irq_obj->ack_irq())
		irq_obj->_ack_irq();
}


Pci::Irq_session_component::Irq_session_component(unsigned irq) : _gsi(irq)
{
	bool valid = false;

	/* invalid irq number for pci devices */
	if (irq == 0xFF)
		return;

	try {
		/* check if IRQ object was used before */
		valid = Proxy::get_irq_proxy<Irq_component>(_gsi, &irq_alloc);
	} catch (Genode::Parent::Service_denied) { }

	if (!valid)
		PERR("unavailable IRQ object 0x%x requested", _gsi);
}


Pci::Irq_session_component::~Irq_session_component()
{
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) return;

	if (_irq_sigh.valid())
		irq_obj->remove_sharer(&_irq_sigh);
}


void Pci::Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) {
		PERR("signal handler got not registered - irq object unavailable");
		return;
	}

	Genode::Signal_context_capability old = _irq_sigh;

	if (old.valid() && !sigh.valid())
		irq_obj->remove_sharer(&_irq_sigh);

	_irq_sigh = sigh;

	if (!old.valid() && sigh.valid())
		irq_obj->add_sharer(&_irq_sigh);
}
