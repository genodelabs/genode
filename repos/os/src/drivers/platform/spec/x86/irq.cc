/*
 * \brief  Implementation of shared IRQs in platform driver
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
#include <util/bit_allocator.h>
#include <base/log.h>
#include <irq_session/connection.h>

/* Platform driver include */
#include "irq.h"
#include "pci_session_component.h"


namespace Platform {
	class Irq_component;
	class Irq_allocator;
	class Irq_thread;
}


using Genode::size_t;
using Genode::addr_t;

/**
 * A simple range allocator implementation used by the Irq_proxy
 */
class Platform::Irq_allocator : public Genode::Range_allocator
{
	private:

		/*
		 * We partition the IRQ space (128 GSIs) into 40 legacy IRQs and 64 MSIs (and
		 * hope the partitions will never overlap on any bizarre platform.)
		 */
		enum { LEGACY = 40, MSI = 64, LEGACY_ARRAY = 64 };

		Genode::Bit_array<LEGACY_ARRAY> _legacy;
		Genode::Bit_allocator<MSI>      _msi;

	public:

		Irq_allocator()
		{
			/* reserve the last 24 legacy IRQs - 40 IRQs remaining */
			_legacy.set(LEGACY, LEGACY_ARRAY - LEGACY);
		}

		unsigned alloc_msi()
		{
			try {
				 return _msi.alloc();
			} catch (Genode::Bit_allocator<MSI>::Out_of_indices) { return ~0U; }
		}

		void free_msi(unsigned msi) { _msi.free(msi); }

		Alloc_return alloc_addr(size_t size, addr_t addr) override
		{
			try {
				_legacy.set(addr, size);
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
 * which we don't want to in the platform driver - one thread is sufficient.
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
class Platform::Irq_thread : public Genode::Thread_deprecated<4096>
{
	private:

		Genode::Signal_receiver _sig_rec;

	public:

		Irq_thread() : Thread_deprecated<4096>("irq_sig_recv") { start(); }

		Genode::Signal_receiver & sig_rec() { return _sig_rec; }

		void entry() {

			typedef Genode::Signal_dispatcher_base Sdb;

			while (1) {
				Genode::Signal sig = _sig_rec.wait_for_signal();

				Sdb *dispatcher = dynamic_cast<Sdb *>(sig.context());

				if (!dispatcher) {
					Genode::error("dispatcher missing for signal ",
					              sig.context(), " ", sig.num());
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
static Platform::Irq_allocator irq_alloc;
static Platform::Irq_thread    irq_thread;


/**
 * Irq_proxy interface implementation
 */
typedef Genode::Irq_proxy<NoThread> Proxy;

class Platform::Irq_component : public Proxy
{
	private:

		Genode::Irq_connection _irq;
		Genode::Signal_dispatcher<Platform::Irq_component> _irq_dispatcher;

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

		virtual bool remove_sharer(Genode::Irq_sigh *s) override {
			if (!Proxy::remove_sharer(s))
				return false;

			/* De-associate handler. */
			_associated = false;
			_irq.sigh(Genode::Signal_context_capability());
			return true;
		}

	public:

		Irq_component(unsigned gsi, Genode::Irq_session::Trigger trigger,
		              Genode::Irq_session::Polarity polarity)
		:
			Proxy(gsi),
			_irq(gsi, trigger, polarity),
			_irq_dispatcher(irq_thread.sig_rec(), *this,
			                &::Proxy::notify_about_irq),
			_associated(false)
		{ }
};



/*******************************
 ** PCI IRQ session component **
 *******************************/

void Platform::Irq_session_component::ack_irq()
{
	if (msi()) {
		Genode::Irq_session_client irq_msi(_irq_cap);
		irq_msi.ack_irq();
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) {
		Genode::error("expected to find IRQ proxy for IRQ ", Genode::Hex(_gsi));
		return;
	}

	if (irq_obj->ack_irq())
		irq_obj->_ack_irq();
}


Platform::Irq_session_component::Irq_session_component(unsigned irq,
                                                       addr_t pci_config_space)
:
	_gsi(irq)
{
	/* invalid irq number for pci_devices */
	if (_gsi >= INVALID_IRQ)
		return;

	if (pci_config_space != ~0UL) {
		/* msi way */
		unsigned msi = irq_alloc.alloc_msi();
		if (msi != ~0U) {
			try {
				using namespace Genode;

				Irq_connection conn(msi, Irq_session::TRIGGER_UNCHANGED,
				                    Irq_session::POLARITY_UNCHANGED,
				                    pci_config_space);

				_msi_info = conn.info();
				if (_msi_info.type == Irq_session::Info::Type::MSI) {
					_gsi     = msi;
					_irq_cap = conn;

					conn.on_destruction(Irq_connection::KEEP_OPEN);
					return;
				}
			} catch (Genode::Parent::Service_denied) { }

			irq_alloc.free_msi(msi);
		}
	}

	Genode::Irq_session::Trigger  trigger;
	Genode::Irq_session::Polarity polarity;

	_gsi = Platform::Irq_override::irq_override(_gsi, trigger, polarity);
	if (_gsi != irq || trigger != Genode::Irq_session::TRIGGER_UNCHANGED ||
	    polarity != POLARITY_UNCHANGED) {

		Genode::log("IRQ override ", irq, "->", _gsi, ", "
		            "trigger mode: ", trigger, ", ", "polarity: ", polarity);
	}

	try {
		/* check if shared IRQ object was used before */
		if (Proxy::get_irq_proxy<Irq_component>(_gsi, &irq_alloc, trigger,
		                                        polarity))
			return;
	} catch (Genode::Parent::Service_denied) { }

	Genode::error("unavailable IRQ ", Genode::Hex(_gsi), " requested");
}


Platform::Irq_session_component::~Irq_session_component()
{
	if (msi()) {
		Genode::Irq_session_client irq_msi(_irq_cap);
		irq_msi.sigh(Genode::Signal_context_capability());

		Genode::env()->parent()->close(_irq_cap);

		irq_alloc.free_msi(_gsi);
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) return;

	if (_irq_sigh.valid())
		irq_obj->remove_sharer(&_irq_sigh);
}


void Platform::Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	if (msi()) {
		/* register signal handler for msi directly at parent */
		Genode::Irq_session_client irq_msi(_irq_cap);
		irq_msi.sigh(sigh);
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Proxy::get_irq_proxy<Irq_component>(_gsi);
	if (!irq_obj) {
		Genode::error("signal handler got not registered - irq object unavailable");
		return;
	}

	Genode::Signal_context_capability old = _irq_sigh;

	if (old.valid() && !sigh.valid())
		irq_obj->remove_sharer(&_irq_sigh);

	_irq_sigh = sigh;

	if (!old.valid() && sigh.valid())
		irq_obj->add_sharer(&_irq_sigh);
}


unsigned short Platform::Irq_routing::rewrite(unsigned char bus, unsigned char dev,
                                         unsigned char func, unsigned char pin)
{
	for (Irq_routing *i = list()->first(); i; i = i->next())
		if ((dev == i->_device) && (pin - 1 == i->_device_pin) &&
		    (i->_bridge_bdf == Platform::bridge_bdf(bus)))
			return i->_gsi;

	return 0;
}
