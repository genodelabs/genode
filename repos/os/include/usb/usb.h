/**
 * \brief  USB session wrapper
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB__USB_H_
#define _INCLUDE__USB__USB_H_

#include <base/allocator.h>
#include <base/entrypoint.h>
#include <usb/types.h>
#include <usb/packet_handler.h>
#include <usb_session/connection.h>
#include <base/log.h>

namespace Usb {

	/* debugging */
	bool constexpr verbose_descr = false;

	class  Device;
	class  Config;
	class  Alternate_interface;
	class  Interface;
	class  Endpoint;
	class  Meta_data;
	class  Sync_completion;
}


class Usb::Meta_data
{
	protected:

		Genode::Allocator * const _md_alloc;
		Connection               &_connection;
		Packet_handler           &_handler;

	public:

		Meta_data(Genode::Allocator * const md_alloc, Connection &device,
		          Packet_handler &handler)
		: _md_alloc(md_alloc), _connection(device), _handler(handler) { }
};

/**
 * Completion for synchronous calls
 */
class Usb::Sync_completion : Completion
{
	private:

		bool               _completed    = false;
		Packet_descriptor &_p;

	public:

		Sync_completion(Packet_handler &handler, Packet_descriptor &p)
		: _p(p)
		{
			Completion *c = p.completion;
			p.completion  = this;

			handler.submit(p);

			while (!_completed)
				handler.wait_for_packet();

			if (c)
				c->complete(p);
		}

		void complete(Packet_descriptor &p) override
		{
			_p         = p;
			_completed = true;
		}
};


class Usb::Endpoint : public Endpoint_descriptor
{
	public:

		Endpoint(Endpoint_descriptor &endpoint_descr)
		: Endpoint_descriptor(endpoint_descr) { }

		bool bulk()      const { return (attributes & 0x3) == ENDPOINT_BULK;      }
		bool interrupt() const { return (attributes & 0x3) == ENDPOINT_INTERRUPT; }

		void dump()
		{
			if (verbose_descr)
				Genode::log("\tEndpoint: "
				            "len: ",        Genode::Hex(length),  " "
				            "type: ",       Genode::Hex(type),    " "
				            "address: ",    Genode::Hex(address), " "
				            "attributes: ", Genode::Hex(attributes));
		}
};


class Usb::Alternate_interface : public Interface_descriptor,
                                 public Meta_data
{
	private:

		enum { MAX_ENDPOINTS = 16 };
		Endpoint *_endpoints[MAX_ENDPOINTS];

		/*
		 * Noncopyable
		 */
		Alternate_interface(Alternate_interface const &);
		Alternate_interface &operator = (Alternate_interface const &);

	public:

		String interface_string { };

		Alternate_interface(Interface_descriptor &interface_desc,
		          Meta_data &md)
		: Interface_descriptor(interface_desc), Meta_data(md)
		{
			dump();

			for (Genode::uint8_t i = 0; i < num_endpoints; i++)
			{
				Endpoint_descriptor descr;
				_connection.endpoint_descriptor(number, alt_settings, i, &descr);
				_endpoints[i] = new (_md_alloc) Endpoint(descr);
				_endpoints[i]->dump();
			}
		}

		~Alternate_interface()
		{
			for (unsigned i = 0; i < num_endpoints; i++)
				destroy(_md_alloc, _endpoints[i]);

			interface_string.free(_md_alloc);
		}

		Endpoint &endpoint(unsigned index)
		{
			if (index >= num_endpoints)
				throw Session::Invalid_endpoint();

			return *_endpoints[index];
		}

		void dump()
		{
			if (!verbose_descr)
				return;

			Genode::warning("Interface: "
			                "len: ",          Genode::Hex(length), " "
			                "type: ",         Genode::Hex(type),   " "
			                "number: ",       Genode::Hex(number), " "
			                "alt_settings: ", Genode::Hex(alt_settings));
			Genode::warning("           "
			                "num_endpoints: ", Genode::Hex(num_endpoints), " "
			                "class: ",         Genode::Hex(iclass),        " ",
			                "subclass: ",      Genode::Hex(isubclass),     " "
			                "protocol: ",      Genode::Hex(iprotocol));
		}
};


class Usb::Interface : public Meta_data
{
	private:

		friend class Config;

		enum { MAX_ALT = 10 };

		Alternate_interface *_interface[MAX_ALT];
		unsigned             _count   = 0;
		Alternate_interface *_current = nullptr;
		bool                 _claimed = false;


		void _check()
		{
			if (!_claimed)
				throw Session::Interface_not_claimed();
		}

	protected:

		void _add(Alternate_interface *iface)
		{
			_interface[_count++] = iface;

			if (iface->active)
				_current = iface;
		}

	public:

		Interface(Meta_data &md)
		: Meta_data(md) { }


		/***************
		 ** Accessors **
		 ***************/

		unsigned             alternate_count() const             { return _count; }
		Alternate_interface &current()                           { return *_current; }

		Alternate_interface &alternate_interface(unsigned index)
		{
			if (index >= _count)
				throw Session::Interface_not_found();

			return *_interface[index];
		}

		Endpoint &endpoint(unsigned index) { return _current->endpoint(index); }


		/***************************
		 ** Packet stream helpers **
		 ***************************/

		Packet_descriptor alloc(size_t size)
		{
			return _handler.alloc(size);
		}

		void submit(Packet_descriptor &p)
		{
			_handler.submit(p);
		}

		void release(Packet_descriptor &p)
		{
			_handler.release(p);
		}

		void *content(Packet_descriptor &p)
		{
			return _handler.content(p);
		}

		/******************************
		 ** Interface to USB service **
		 ******************************/

		/**
		 * Claim interface
		 */
		void claim()
		{
			_connection.claim_interface(_interface[0]->number);
			_claimed = true;
		}

		/**
		 * Release interface
		 */
		void release()
		{
			if (_claimed)
				return;

			Packet_descriptor p = alloc(0);
			p.type              = Packet_descriptor::RELEASE_IF;
			p.number            = _interface[0]->number;
			p.succeded          = false;

			Sync_completion sync(_handler, p);

			if (p.succeded)
				_claimed = false;
		}

		void set_alternate_interface(Alternate_interface &alternate)
		{
			_check();

			Packet_descriptor p     = alloc(0);
			p.type                  = Packet_descriptor::ALT_SETTING;
			p.succeded              = false;
			p.interface.number      = alternate.number;
			p.interface.alt_setting = alternate.alt_settings;

			Sync_completion sync(_handler, p);

			if (p.succeded)
				_current = _interface[p.interface.alt_setting];
		}

		void control_transfer(Packet_descriptor &p, uint8_t request_type, uint8_t request,
		                      uint16_t value, uint16_t index, int timeout,
		                      bool block = true, Completion *c = nullptr)
		{
			_check();

			p.type                 = Usb::Packet_descriptor::CTRL;
			p.succeded             = false;
			p.control.request      = request;
			p.control.request_type = request_type;
			p.control.value        = value;
			p.control.index        = index;
			p.control.timeout      = timeout;
			p.completion           = c;

			if(block) Sync_completion sync(_handler, p);
			else _handler.submit(p);
		}

		void bulk_transfer(Packet_descriptor &p, Endpoint &ep,
		                   bool block = true, Completion *c = nullptr)
		{
			_check();

			if (!ep.bulk())
				throw Session::Invalid_endpoint();

			p.type              = Usb::Packet_descriptor::BULK;
			p.succeded          = false;
			p.transfer.ep       = ep.address;
			p.completion        = c;

			if(block) Sync_completion sync(_handler, p);
			else _handler.submit(p);
		}

		void interrupt_transfer(Packet_descriptor &p, Endpoint &ep,
		                        int polling_interval =
		                        	Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL,
		                        bool block = true, Completion *c = nullptr)
		{
			_check();

			if (!ep.interrupt())
				throw Session::Invalid_endpoint();

			p.type                      = Usb::Packet_descriptor::IRQ;
			p.succeded                  = false;
			p.transfer.ep               = ep.address;
			p.transfer.polling_interval = polling_interval;
			p.completion                = c;

			if(block) Sync_completion sync(_handler, p);
			else _handler.submit(p);
		}
};


class Usb::Config : public Config_descriptor,
                    public Meta_data
{
	private:

		enum { MAX_INTERFACES = 32 };

		/*
		 * Noncopyable
		 */
		Config(Config const &);
		Config &operator = (Config const &);

		Interface *_interfaces[MAX_INTERFACES];
		unsigned   _total_interfaces = 0;

	public:

		String config_string { };

		Config(Config_descriptor &config_desc, Meta_data &md)
		: Config_descriptor(config_desc), Meta_data(md)
		{
			dump();

			for (Genode::uint8_t i = 0; i < num_interfaces; i++) {

				Interface_descriptor descr;
				_connection.interface_descriptor(i, 0, &descr);
				_interfaces[descr.number] = new(_md_alloc) Interface(md);

				/* read number of alternative settings */
				unsigned alt_settings = _connection.alt_settings(i);
				_total_interfaces    += alt_settings;

				/* alt settings */
				for (unsigned j = 0; j < alt_settings; j++) {
					_connection.interface_descriptor(i, j, &descr);
					if (descr.number != i)
						error("Interface number != index");

					_interfaces[descr.number]->_add(new(_md_alloc) Alternate_interface(descr, md));
				}
			}
		}

		~Config()
		{
			for (unsigned i = 0; i < num_interfaces; i++)
				destroy(_md_alloc, _interfaces[i]);

			config_string.free(_md_alloc);
		}

		Interface &interface(unsigned num)
		{

			if (num >= num_interfaces)
				throw Session::Interface_not_found();

			return *_interfaces[num];
		}

		void dump()
		{
			if (verbose_descr)
				log("Config: "
				    "len: ",          Genode::Hex(length),         " "
				    "type: ",         Genode::Hex(type),           " "
				    "total_length: ", Genode::Hex(total_length),   " "
				    "num_intf: ",     Genode::Hex(num_interfaces), " "
				    "config_value: ", Genode::Hex(config_value));
		}
};


class Usb::Device : public Meta_data
{
	private:

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);

		Packet_handler _handler;

		void _clear()
		{
			if (!config)
				return;

			manufactorer_string.free(_md_alloc);
			product_string.free(_md_alloc);
			serial_number_string.free(_md_alloc);
			destroy(_md_alloc, config);
		}

	public:

		enum Speed {
			SPEED_UNKNOWN = 0,                  /* enumerating */
			SPEED_LOW,
			SPEED_FULL,                         /* usb 1.1 */
			SPEED_HIGH,                         /* usb 2.0 */
			SPEED_WIRELESS,                     /* wireless (usb 2.5) */
			SPEED_SUPER,                        /* usb 3.0 */
		};

		Device_descriptor device_descr { };

		Config *config = nullptr;

		String manufactorer_string  { };
		String product_string       { };
		String serial_number_string { };

		Device(Genode::Allocator * const md_alloc, Connection &connection,
		       Genode::Entrypoint &ep)
		: Meta_data(md_alloc, connection, _handler), _handler(connection, ep)
		{ }

		Device_descriptor const *descriptor()        { return &device_descr; }
		Config                  *config_descriptor() { return config; }

		char const *speed_string(unsigned speed)
		{
			switch (speed) {
				case SPEED_LOW     : return "LOW";
				case SPEED_FULL    : return "FULL";
				case SPEED_HIGH    : return "HIGH";
				case SPEED_WIRELESS: return "WIRELESS";
				case SPEED_SUPER   : return "SUPER";

				default: return "<unknown>";
			}
		}

		void string_descriptor(uint8_t index, String *target)
		{
			Packet_descriptor p = _handler.alloc(128);
			p.type          = Packet_descriptor::STRING;
			p.string.index  = index;
			p.string.length = 128;

			Sync_completion sync(_handler, p);
			target->copy(p.string.length, _handler.content(p), _md_alloc);
		}

		/**
		 * Re-read all descriptors (device, config, interface, and endpoints)
		 * Must be called before Usb::Device can be used (asynchronous)
		 */
		void update_config()
		{
			/* free info from previous call */
			_clear();

			Config_descriptor config_descr;

			_connection.config_descriptor(&device_descr, &config_descr);
			dump();

			config = new (_md_alloc) Config(config_descr, *this);

			/* retrieve string descriptors */
			string_descriptor(device_descr.manufactorer_index, &manufactorer_string);
			string_descriptor(device_descr.product_index, &product_string);
			string_descriptor(device_descr.serial_number_index, &serial_number_string);
			string_descriptor(config->config_index, &config->config_string);

			for (unsigned i = 0; i < config->num_interfaces; i++) {
				Interface &iface = config->interface(i);

				for (unsigned j = 0; j < iface.alternate_count(); j++)
					string_descriptor(iface.alternate_interface(j).interface_index,
					                  &iface.alternate_interface(j).interface_string);
			}
		}

		/**
		 * Set configuration, no interfaces can be claimed (asynchronous)
		 */
		void set_configuration(uint8_t num)
		{
			if (!config) {
				Genode::error("No current configuration found");
				return;
			}

			if (!num || num > device_descr.num_configs) {
				Genode::error("Valid configuration values: 1 ... ", device_descr.num_configs);
				return;
			}

			if (config && num == config->config_value)
				return;

			Packet_descriptor p = _handler.alloc(0);
			p.type              = Packet_descriptor::CONFIG;
			p.number            = num;

			Sync_completion sync(_handler, p);

			if (p.succeded)
				update_config();
		}

		Interface &interface(unsigned interface_num)
		{
			return config->interface(interface_num);
		}

		void dump()
		{
			if (!verbose_descr)
				return;

			using Genode::Hex;

			Genode::log("Device: "
			            "len: ",        Hex(device_descr.length),    " "
			            "type: " ,      Hex(device_descr.type),      " "
			            "class: ",      Hex(device_descr.dclass),    " "
			            "sub-class: ",  Hex(device_descr.dsubclass), " "
			            "proto: ",      Hex(device_descr.dprotocol), " "
			            "max_packet: ", Hex(device_descr.max_packet_size));
			Genode::log("        "
			            "vendor: ",     Hex(device_descr.vendor_id),  " "
			            "product: ",    Hex(device_descr.product_id), " "
			            "configs: ",    Hex(device_descr.num_configs));
		}
};

#endif /* _INCLUDE__USB__USB_H_ */
