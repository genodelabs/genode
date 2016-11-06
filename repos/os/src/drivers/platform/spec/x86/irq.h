/*
 * \brief  IRQ session interface
 * \author Alexander Boettcher
 * \date   2015-03-25
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#include <base/rpc_server.h>
#include <util/list.h>
#include <irq_session/connection.h>

/* platform local includes */
#include <irq_proxy.h>


namespace Platform {
	class Irq_session_component;
	class Irq_override;
	class Irq_routing;
}


class Platform::Irq_session_component : public Genode::Rpc_object<Genode::Irq_session>,
                                        public Genode::List<Irq_session_component>::Element
{
	private:

		unsigned                  _gsi;
		Genode::Irq_sigh          _irq_sigh;
		Genode::Irq_session::Info _msi_info;

		Genode::Lazy_volatile_object<Genode::Irq_connection> _irq_conn;

	public:

		enum { INVALID_IRQ = 0xffU };

		Irq_session_component(unsigned, Genode::addr_t);
		~Irq_session_component();

		bool msi()
		{
			return _irq_conn.constructed() &&
			       _msi_info.type == Genode::Irq_session::Info::Type::MSI;
		}

		unsigned gsi() { return _gsi; }

		unsigned long msi_address() const { return _msi_info.address; }
		unsigned long msi_data()    const { return _msi_info.value; }

		/***************************
		 ** Irq session interface **
		 ***************************/

		void ack_irq() override;
		void sigh(Genode::Signal_context_capability) override;
		Info info() override { 
			return { .type = Genode::Irq_session::Info::Type::INVALID }; }
};


/**
 * List that holds interrupt override information
 */
class Platform::Irq_override : public Genode::List<Platform::Irq_override>::Element
{
	private:

		unsigned short _irq;   /* source IRQ */
		unsigned short _gsi;   /* target GSI */
		Genode::Irq_session::Trigger _trigger; /* interrupt trigger mode */
		Genode::Irq_session::Polarity _polarity; /* interrupt polarity */

		Genode::Irq_session::Trigger _mode2trigger(unsigned mode)
		{
			enum { EDGE = 0x4, LEVEL = 0xc };

			switch (mode & 0xc) {
				case EDGE:
					return Genode::Irq_session::TRIGGER_EDGE;
				case LEVEL:
					return Genode::Irq_session::TRIGGER_LEVEL;
				default:
					return Genode::Irq_session::TRIGGER_UNCHANGED;
			}
		}

		Genode::Irq_session::Polarity _mode2polarity(unsigned mode)
		{
			using namespace Genode;
			enum { HIGH = 0x1, LOW = 0x3 };

			switch (mode & 0x3) {
				case HIGH:
					return Genode::Irq_session::POLARITY_HIGH;
				case LOW:
					return Genode::Irq_session::POLARITY_LOW;
				default:
					return Genode::Irq_session::POLARITY_UNCHANGED;
			}
		}

	public:

		Irq_override(unsigned irq, unsigned gsi, unsigned mode)
		:
		  _irq(irq), _gsi(gsi),
		  _trigger(_mode2trigger(mode)), _polarity(_mode2polarity(mode))
		{ }

		static Genode::List<Irq_override> *list()
		{
			static Genode::List<Irq_override> _list;
			return &_list;
		}

		unsigned short irq()                     const { return _irq; }
		unsigned short gsi()                     const { return _gsi; }
		Genode::Irq_session::Trigger trigger()   const { return _trigger; }
		Genode::Irq_session::Polarity polarity() const { return _polarity; }

		static unsigned irq_override (unsigned irq,
		                              Genode::Irq_session::Trigger &trigger,
		                              Genode::Irq_session::Polarity &polarity)
		{
			for (Irq_override *i = list()->first(); i; i = i->next())
				if (i->irq() == irq) {
					trigger  = i->trigger();
					polarity = i->polarity();
					return i->gsi();
				}

			trigger  = Genode::Irq_session::TRIGGER_UNCHANGED;
			polarity = Genode::Irq_session::POLARITY_UNCHANGED;
			return irq;
		}
};


/**
 * List that holds interrupt rewrite information
 */
class Platform::Irq_routing : public Genode::List<Platform::Irq_routing>::Element
{
	private:

		unsigned short _gsi;
		unsigned short _bridge_bdf;
		unsigned short _device;
		unsigned char  _device_pin;

	public:

		static Genode::List<Irq_routing> *list()
		{
			static Genode::List<Irq_routing> _list;
			return &_list;
		}

		Irq_routing(unsigned short gsi, unsigned short bridge_bdf,
		            unsigned char device, unsigned char device_pin)
		:
			_gsi(gsi), _bridge_bdf(bridge_bdf), _device(device),
			_device_pin(device_pin)
		{ }

		static unsigned short rewrite(unsigned char bus, unsigned char dev,
		                              unsigned char func, unsigned char pin);
};
