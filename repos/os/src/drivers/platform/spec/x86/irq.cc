/*
 * \brief  Implementation of shared IRQs in platform driver
 * \author Alexander Boettcher
 * \date   2015-03-27
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
}


using Genode::size_t;
using Genode::addr_t;

/**
 * A simple allocator implementation used by the Irq_proxy
 */
class Platform::Irq_allocator
{
	private:

		/*
		 * We partition the IRQ space (128 GSIs) into 40 legacy IRQs and 64 MSIs (and
		 * hope the partitions will never overlap on any bizarre platform.)
		 */
		enum { LEGACY = 40, MSI = 64, LEGACY_ARRAY = 64 };

		Genode::Bit_array<LEGACY_ARRAY> _legacy { };
		Genode::Bit_allocator<MSI>      _msi    { };

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

		bool alloc_irq(addr_t addr)
		{
			try {
				_legacy.set(addr, 1);
				return true;
			} catch (...) {
				return false;
			}
		}
};


/**
 * One allocator for managing in use IRQ numbers and one IRQ thread waiting
 * for Genode signals of all hardware IRQs.
 */
static Platform::Irq_allocator irq_alloc;


/**
 * Irq_proxy interface implementation
 */
class Platform::Irq_component : public Platform::Irq_proxy
{
	private:

		Genode::Irq_connection _irq;
		Genode::Signal_handler<Platform::Irq_component> _irq_dispatcher;

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

		virtual bool remove_sharer(Platform::Irq_sigh *s) override {
			if (!Irq_proxy::remove_sharer(s))
				return false;

			/* De-associate handler. */
			_associated = false;
			_irq.sigh(Genode::Signal_context_capability());
			return true;
		}

	public:

		Irq_component(Genode::Env &env, unsigned gsi,
		              Genode::Irq_session::Trigger trigger,
		              Genode::Irq_session::Polarity polarity)
		:
			Irq_proxy(gsi),
			_irq(env, gsi, trigger, polarity),
			_irq_dispatcher(env.ep(), *this, &Platform::Irq_proxy::notify_about_irq),
			_associated(false)
		{ }

		static Irq_component *get_irq_proxy(unsigned irq_number,
		                                    Irq_allocator *irq_alloc = nullptr,
		                                    Genode::Irq_session::Trigger trigger = Genode::Irq_session::TRIGGER_UNCHANGED,
		                                    Genode::Irq_session::Polarity polarity = Genode::Irq_session::POLARITY_UNCHANGED,
		                                    Genode::Env *env = nullptr,
		                                    Genode::Allocator *heap = nullptr)
		{
			static Genode::List<Irq_proxy> proxies;
			static Genode::Lock            proxies_lock;

			Genode::Lock::Guard lock_guard(proxies_lock);

			/* lookup proxy in database */
			for (Irq_proxy *p = proxies.first(); p; p = p->next())
				if (p->irq_number() == irq_number)
					return static_cast<Irq_component *>(p);

			/* try to create proxy */
			if (!irq_alloc || !env || !heap || !irq_alloc->alloc_irq(irq_number))
				return 0;

			Irq_component *new_proxy = new (heap) Irq_component(*env, irq_number, trigger,
			                                                    polarity);
			proxies.insert(new_proxy);
			return new_proxy;
		}
};



/*******************************
 ** PCI IRQ session component **
 *******************************/

void Platform::Irq_session_component::ack_irq()
{
	if (msi()) {
		_irq_conn->ack_irq();
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Irq_component::get_irq_proxy(_gsi);
	if (!irq_obj) {
		Genode::error("expected to find IRQ proxy for IRQ ", Genode::Hex(_gsi));
		return;
	}

	if (irq_obj->ack_irq())
		irq_obj->_ack_irq();
}


Platform::Irq_session_component::Irq_session_component(unsigned irq,
                                                       addr_t pci_config_space,
                                                       Genode::Env       &env,
                                                       Genode::Allocator &heap)
:
	_gsi(irq)
{
	if (pci_config_space != ~0UL) {
		/* msi way */
		unsigned msi = irq_alloc.alloc_msi();
		if (msi != ~0U) {
			try {
				using namespace Genode;

				_irq_conn.construct(env, msi, Irq_session::TRIGGER_UNCHANGED,
				                    Irq_session::POLARITY_UNCHANGED,
				                    pci_config_space);

				_msi_info = _irq_conn->info();
				if (_msi_info.type == Irq_session::Info::Type::MSI) {
					_gsi = msi;
					return;
				}
			} catch (Genode::Service_denied) { }

			irq_alloc.free_msi(msi);
		}
	}

	/* invalid irq number for pci_devices */
	if (_gsi >= INVALID_IRQ)
		return;

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
		if (Irq_component::get_irq_proxy(_gsi, &irq_alloc, trigger,
		                                 polarity, &env, &heap))
			return;
	} catch (Genode::Service_denied) { }

	Genode::error("unavailable IRQ ", Genode::Hex(_gsi), " requested");
}


Platform::Irq_session_component::~Irq_session_component()
{
	if (msi()) {
		_irq_conn->sigh(Genode::Signal_context_capability());

		irq_alloc.free_msi(_gsi);
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Irq_component::get_irq_proxy(_gsi);
	if (!irq_obj) return;

	if (_irq_sigh.valid())
		irq_obj->remove_sharer(&_irq_sigh);
}


void Platform::Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	if (_irq_conn.constructed()) {
		/* register signal handler for msi directly at parent */
		_irq_conn->sigh(sigh);
		return;
	}

	/* shared irq handling */
	Irq_component *irq_obj = Irq_component::get_irq_proxy(_gsi);
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
                                              unsigned char, unsigned char pin)
{
	unsigned const bridge_bdf_bus = Platform::bridge_bdf(bus);

	for (Irq_routing *i = list()->first(); i; i = i->next()) {
		if ((dev == i->_device) && (pin - 1 == i->_device_pin) &&
		    (i->_bridge_bdf == bridge_bdf_bus))
			return i->_gsi;
	}

	return 0;
}
