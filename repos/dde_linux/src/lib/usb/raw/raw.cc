/**
 * \brief  Server side USB session implementation
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <os/reporter.h>
#include <root/component.h>
#include <usb_session/rpc_object.h>
#include <util/list.h>

#include <extern_c_begin.h>
#include <linux/usb.h>
#include "raw.h"
#include <extern_c_end.h>
#include <signal.h>

using namespace Genode;

extern "C" int usb_set_configuration(struct usb_device *dev, int configuration);

constexpr bool verbose_raw = false;


namespace Usb {
	class  Session_component;
	class  Root;
	class  Worker;
}

/**
 * Keep track of all registered USB devices (via raw driver)
 */
struct Device : List<Device>::Element
{
	usb_device *udev;

	static List<Device> *list()
	{
		static List<Device> _l;
		return &_l;
	}

	static Device * device(uint16_t vendor, uint16_t product)
	{
		for (Device *d = list()->first(); d; d = d->next()) {
			if (d->udev->descriptor.idVendor == vendor && d->udev->descriptor.idProduct == product)
				return d;
		}

		return nullptr;
	}

	static Genode::Reporter &device_list_reporter()
	{
		static Genode::Reporter _r("devices", 512*1024);
		return _r;
	}

	static void report_device_list()
	{
		Genode::Reporter::Xml_generator xml(device_list_reporter(), [&] ()
		{

			for (Device *d = list()->first(); d; d = d->next()) {
				xml.node("device", [&] ()
				{
					char buf[7];

					Genode::snprintf(buf, sizeof(buf), "0x%4x",
					                 d->udev->descriptor.idVendor);
					xml.attribute("vendor_id", buf);

					Genode::snprintf(buf, sizeof(buf), "0x%4x",
					                 d->udev->descriptor.idProduct);
					xml.attribute("product_id", buf);
				});
			}
		});
	}

	Device(usb_device *udev) : udev(udev)
	{
		list()->insert(this);

		if (device_list_reporter().is_enabled())
			report_device_list();
	}

	~Device()
	{
		list()->remove(this);

		if (device_list_reporter().is_enabled())
			report_device_list();
	}

	usb_interface *interface(unsigned index)
	{
		usb_interface *iface = udev->actconfig->interface[index];
		return iface;
	}

	usb_host_endpoint *endpoint(usb_interface *iface, unsigned alt_setting,
	                            unsigned endpoint_num)
	{
		return &iface->altsetting[alt_setting].endpoint[endpoint_num];
	}
};


/**
 * Handle packet stream request, this way the entrypoint always returns to it's
 * server loop
 */
class Usb::Worker
{
	private:

		completion _packet_avail;

		Session::Tx::Sink        *_sink;
		Device                   *_device      = nullptr;
		Signal_context_capability _sigh_ready;
		Routine                  *_routine     = nullptr;
		unsigned                  _p_in_flight = 0;
		bool                      _device_ready = false;

		void _ack_packet(Packet_descriptor &p)
		{
			_sink->acknowledge_packet(p);
			_p_in_flight--;
		}

		/**
		 * Retrieve string descriptor at index given in packet
		 */
		void _retrieve_string(Packet_descriptor &p)
		{
			char *buffer = _sink->packet_content(p);
			int   length;

			if ((length = usb_string(_device->udev, p.string.index, buffer, p.size())) < 0) {
				PWRN("Could not read string descriptor index: %u", p.string.index);
				p.string.length = 0;
			} else {
				/* returned length is in bytes (char) */
				p.string.length   = length / 2;
				p.succeded        = true;
			}
		}

		/**
		 * Read control transfer
		 */
		void _ctrl_in(Packet_descriptor &p)
		{
			void *buf = kmalloc(4096, GFP_NOIO);

			int err = usb_control_msg(_device->udev, usb_rcvctrlpipe(_device->udev, 0),
			                          p.control.request, p.control.request_type,
			                          p.control.value, p.control.index, buf,
			                          p.size(), p.control.timeout);

			if (err > 0 && p.size())
				Genode::memcpy(_sink->packet_content(p), buf, err);

			kfree(buf);

			if (err < 0 && err != -EPIPE) {
				p.succeded = false;
				return;
			}

			p.control.actual_size = err;
			p.succeded = true;
		}

		/**
		 * Write control transfer
		 */
		void _ctrl_out(Packet_descriptor &p)
		{
			void *buf = kmalloc(4096, GFP_NOIO);

			if (p.size())
				Genode::memcpy(buf, _sink->packet_content(p), p.size());

			int err = usb_control_msg(_device->udev, usb_sndctrlpipe(_device->udev, 0),
			                          p.control.request, p.control.request_type,
			                          p.control.value, p.control.index, buf, p.size(),
			                          p.control.timeout);
			if (err >= 0 || err== -EPIPE) {
				p.control.actual_size = err;
				p.succeded = true;
			}

			kfree(buf);
		}

		/**
		 * Asynchronous transfer helpers
		 */
		struct Complete_data
		{
			Worker            *worker;
			Packet_descriptor  packet;
		};

		void _async_finish(Packet_descriptor &p, urb *urb, bool read)
		{
			if (urb->status == 0) {
				p.transfer.actual_size = urb->actual_length;
				p.succeded             = true;

				if (read)
					Genode::memcpy(_sink->packet_content(p), urb->transfer_buffer, 
				               	   urb->actual_length);
			}

			_ack_packet(p);
		}

		static void _async_complete(urb *urb)
		{
			Complete_data *data = (Complete_data *)urb->context;

			data->worker->_async_finish(data->packet, urb,
			                            !!(data->packet.transfer.ep & USB_DIR_IN));
			kfree (data);
			kfree (urb->transfer_buffer);
			usb_free_urb(urb);
		}

		/**
		 * Bulk transfer
		 */
		bool _bulk(Packet_descriptor &p, bool read)
		{
			unsigned pipe;
			void    *buf = kmalloc(p.size(), GFP_NOIO);

			if (read)
				pipe = usb_rcvbulkpipe(_device->udev, p.transfer.ep);
			else {
				pipe = usb_sndbulkpipe(_device->udev, p.transfer.ep);
				Genode::memcpy(buf, _sink->packet_content(p), p.size());
			}

			urb *bulk_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!bulk_urb) {
				PERR("Failed to allocate bulk URB");
				kfree(buf);
				return false;
			}

			Complete_data *data = (Complete_data *)kmalloc(sizeof(Complete_data), GFP_KERNEL);
			data->packet   = p;
			data->worker   = this;

			usb_fill_bulk_urb(bulk_urb, _device->udev, pipe, buf, p.size(),
			                 _async_complete, data);

			if (usb_submit_urb(bulk_urb, GFP_KERNEL))
				PERR("Failed to submit URB");

			return true;
		}

		/**
		 * IRQ transfer
		 */
		bool _irq(Packet_descriptor &p, bool read)
		{
			unsigned pipe;
			void    *buf = kmalloc(p.size(), GFP_NOIO);

			if (read)
				pipe = usb_rcvintpipe(_device->udev, p.transfer.ep);
			else {
				pipe = usb_sndintpipe(_device->udev, p.transfer.ep);
				Genode::memcpy(buf, _sink->packet_content(p), p.size());
			}

			urb *irq_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!irq_urb) {
				PERR("Failed to allocate interrupt URB");
				kfree(buf);
				return false;
			}

			Complete_data *data = (Complete_data *)kmalloc(sizeof(Complete_data), GFP_KERNEL);
			data->packet   = p;
			data->worker   = this;

			usb_fill_int_urb(irq_urb, _device->udev, pipe, buf, p.size(),
			                 _async_complete, data, p.transfer.timeout);

			if (usb_submit_urb(irq_urb, GFP_KERNEL))
				PERR("Failed to submit URB");

			return true;
		}

		/**
		 * Change alternate settings for device
		 */
		void _alt_setting(Packet_descriptor &p)
		{
			int err = usb_set_interface(_device->udev, p.interface.number,
			                            p.interface.alt_setting);
			if (!err)
				p.succeded = true;
		}

		/**
		 * Set configuration
		 */
		void _config(Packet_descriptor &p)
		{
			usb_host_config *config = _device->udev->actconfig;

			if (!config)
				return;

			for (unsigned i = 0; i < config->desc.bNumInterfaces; i++) {
				if (usb_interface_claimed(config->interface[i])) {
					PERR("There are interfaces claimed, won't set configuration");
					return;
				}
			}

			int err = usb_set_configuration(_device->udev, p.number);

			if (!err)
				p.succeded = true;
		}

		/**
		 * Release interface
		 */
		void _release_interface(Packet_descriptor &p)
		{
			usb_interface *iface = _device->interface(p.number);

			if (!iface)
				return;

			usb_driver_release_interface(&raw_intf_driver, iface);
			p.succeded = true;
		}

		/**
		 * Dispatch incoming packet types
		 */
		void _dispatch()
		{
			/*
			 * Get packets until there are no more free ack slots or avaiable
			 * packets
			 */
			while (_p_in_flight < _sink->ack_slots_free() && _sink->packet_avail())
			{
				Packet_descriptor p = _sink->get_packet();

				if (verbose_raw)
					PDBG("PACKET: %u first value: %x", p.type, p.number);

				_p_in_flight++;

				if (!_device || !_device->udev) {
					_ack_packet(p);
					continue;
				}

				switch (p.type) {

					case Packet_descriptor::STRING:
						_retrieve_string(p);
						break;

					case Packet_descriptor::CTRL:
						if (p.control.request_type & Usb::ENDPOINT_IN)
							_ctrl_in(p);
						else
							_ctrl_out(p);
						break;

					case Packet_descriptor::BULK:
						if (_bulk(p, !!(p.transfer.ep & USB_DIR_IN)))
							continue;

					case Packet_descriptor::IRQ:
						if (_irq(p, !!(p.transfer.ep & USB_DIR_IN)))
							continue;

					case Packet_descriptor::ALT_SETTING:
						_alt_setting(p);
						break;

					case Packet_descriptor::CONFIG:
						_config(p);
						break;

					case Packet_descriptor::RELEASE_IF:
						_release_interface(p);
						break;
				}

				_ack_packet(p);
			}
		}

		void _wait_for_device()
		{
			_wait_event(_device);
			_wait_event(_device->udev->actconfig);

			if (_sigh_ready.valid())
				Signal_transmitter(_sigh_ready).submit(1);

			_device_ready = true;
		}

		/**
		 * Wait for packets
		 */
		void _wait()
		{
			/* wait for device to become ready */
			init_completion(&_packet_avail);

			_wait_for_device();

			while (true) {
				wait_for_completion(&_packet_avail);

				_dispatch();
				Routine::schedule_all();
			}
		}

	public:

		static int run(void *worker)
		{
			Worker *w = static_cast<Worker *>(worker);
			w->_wait();
			return 0;
		}

		Worker(Session::Tx::Sink *sink)
		: _sink(sink)
		{ }

		void start()
		{
			if (!_routine) {
				_routine = Routine::add(run, this, "worker");
				Routine::schedule_all();
			}
		}

		void stop()
		{
			if (_routine)
				Routine::remove(_routine);
			_routine = nullptr;
		}

		void packet_avail() { ::complete(&_packet_avail); }

		void device(Device *device, Signal_context_capability sigh_ready = Signal_context_capability())
		{
			_device       = device;
			_sigh_ready   = sigh_ready;
		}

		bool device_ready() { return _device_ready; }
};


/*****************
 ** USB session **
 *****************/

class Usb::Session_component : public Session_rpc_object,
                               public List<Session_component>::Element
{
	private:

		Server::Entrypoint                  &_ep;
		unsigned long                        _vendor;
		unsigned long                        _product;
		Device                              *_device = nullptr;
		Signal_context_capability            _sigh_state_change;
		Signal_rpc_member<Session_component> _packet_avail;
		Signal_rpc_member<Session_component> _ready_ack;
		Worker                               _worker;


		void _signal_state_change()
		{
			if (_sigh_state_change.valid())
				Signal_transmitter(_sigh_state_change).submit(1);
		}

		void _receive(unsigned)
		{
			_worker.packet_avail();
		}

	public:

		enum State {
			DEVICE_ADD,
			DEVICE_REMOVE,
		};

		Session_component(Genode::Dataspace_capability tx_ds, Server::Entrypoint &ep,
		                  unsigned long vendor, unsigned long product)
		: Session_rpc_object(tx_ds, ep.rpc_ep()),
		  _ep(ep),
		  _vendor(vendor), _product(product),
		  _packet_avail(ep, *this, &Session_component::_receive),
		  _ready_ack(ep, *this, &Session_component::_receive),
		  _worker(sink())
		{
			Device *device = Device::device(_vendor, _product);
			if (device) {
				PDBG("Found device");
				state_change(DEVICE_ADD, device);
			}

			/* register signal handlers */
			_tx.sigh_packet_avail(_packet_avail);
		}

		~Session_component()
		{
			_worker.stop();
		}

		/***********************
		 ** Session interface **
		 ***********************/

		bool plugged() { return _device != nullptr; }

		void claim_interface(unsigned interface_num) override
		{
			usb_interface *iface   = _device->interface(interface_num);
			if (!iface)
				throw Interface_not_found();

			if (usb_driver_claim_interface(&raw_intf_driver, iface, nullptr))
				throw Interface_already_claimed();
		}

		void config_descriptor(Device_descriptor *device_descr,
		                       Config_descriptor *config_descr) override
		{
			Genode::memcpy(device_descr, &_device->udev->descriptor,   sizeof(usb_device_descriptor));
			Genode::memcpy(config_descr, &_device->udev->actconfig->desc, sizeof(usb_config_descriptor));

			device_descr->bus   = _device->udev->bus->busnum;
			device_descr->num   = _device->udev->devnum;
			device_descr->speed = _device->udev->speed;
		}

		unsigned alt_settings(unsigned index) override
		{
			return _device->interface(index)->num_altsetting;
		}

		void interface_descriptor(unsigned index, unsigned alt_setting,
		                          Interface_descriptor *interface_descr) override
		{
			if (!_device)
				throw Device_not_found();

			if (index >= _device->udev->actconfig->desc.bNumInterfaces)
				throw Interface_not_found();

			usb_interface *iface = _device->interface(index);
			Genode::memcpy(interface_descr, &iface->altsetting[alt_setting].desc,
			               sizeof(usb_interface_descriptor));

			if (&iface->altsetting[alt_setting] == iface->cur_altsetting)
				interface_descr->active = true;
		}

		void endpoint_descriptor(unsigned              interface_num,
		                         unsigned              alt_setting,
		                         unsigned              endpoint_num,
		                         Endpoint_descriptor  *endpoint_descr) override
		{
			if (!_device)
				throw Device_not_found();

			if (interface_num >= _device->udev->actconfig->desc.bNumInterfaces)
				throw Interface_not_found();

			usb_interface *iface = usb_ifnum_to_if(_device->udev, interface_num);
			Genode::memcpy(endpoint_descr, &_device->endpoint(iface, alt_setting,
			               endpoint_num)->desc, sizeof(usb_endpoint_descriptor));
		}

		/*********************
		 ** Local interface **
		 *********************/

		bool session_device(Device *device)
		{
			usb_device_descriptor *descr = &device->udev->descriptor;
			return (descr->idVendor == _vendor && descr->idProduct == _product) ? true : false;
		}

		bool state_change(State state, Device *device)
		{
			switch (state) {
				case DEVICE_ADD:
					if (!session_device(device))
						return false;

					if (_device)
						PWRN("Device type already present (vendor: %x product: %x). Overwrite!",
						     device->udev->descriptor.idVendor,
						     device->udev->descriptor.idProduct);

					_device = device;
					_worker.device(_device, _sigh_state_change);
					_worker.start();
					return true;

				case DEVICE_REMOVE:
					if (!session_device(device))
						return false;
					_device = nullptr;
					_worker.stop();
					_signal_state_change();
					return true;
			}

			return false;
		}

		void sigh_state_change(Signal_context_capability sigh)
		{
			_sigh_state_change = sigh;

			if (_worker.device_ready())
				Signal_transmitter(_sigh_state_change).submit(1);
		}
};


struct Session : public List<Usb::Session_component>
{
	static Session *list()
	{
		static Session _l;
		return &_l;
	}

	void state_change(Usb::Session_component::State state, Device *device)
	{
		for (Usb::Session_component *session = list()->first(); session; session = session->next())
			if (session->state_change(state, device))
				return;
	}
};


class Usb::Root : public Genode::Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			unsigned long vendor  = Arg_string::find_arg(args, "vendor").ulong_value(0);
			unsigned long product = Arg_string::find_arg(args, "product").ulong_value(0);

			/* check session quota */
			size_t session_size = max<size_t>(4096, sizeof(Session_component));
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			if (tx_buf_size > ram_quota - session_size) {
				PERR("Insufficient 'ram_quota',got %zu, need %zu",
				     ram_quota, tx_buf_size + session_size);
				throw Root::Quota_exceeded();
			}

			Dataspace_capability tx_ds = env()->ram_session()->alloc(tx_buf_size);
			Session_component *session =  new (md_alloc())
			                              Session_component(tx_ds, _ep, vendor, product);
			::Session::list()->insert(session);
			return session;
		}

		void _destroy_session(Session_component *session)
		{
			::Session::list()->remove(session);
			Genode::Root_component<Session_component>::_destroy_session(session);
		}

	public:

		Root(Server::Entrypoint &session_ep,
		     Genode::Allocator *md_alloc)
		: Genode::Root_component<Session_component>(&session_ep.rpc_ep(), md_alloc),
		  _ep(session_ep) { }
};


void Raw::init(Server::Entrypoint &ep, bool report_device_list)
{
	Device::device_list_reporter().enabled(report_device_list);
	static Usb::Root root(ep, env()->heap());
	Genode::env()->parent()->announce(ep.rpc_ep().manage(&root));
}


/*****************
 ** C interface **
 *****************/

int raw_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	struct usb_device *udev = (struct usb_device*)data;

	switch (action) {

		case USB_DEVICE_ADD:
		{
			::Session::list()->state_change(Usb::Session_component::DEVICE_ADD,
                                			new (env()->heap()) Device(udev));
			break;
		}

		case USB_DEVICE_REMOVE:
		{
			Device *dev = Device::device(udev->descriptor.idVendor,
	                             	 	 udev->descriptor.idProduct);
			if (dev) {
				::Session::list()->state_change(Usb::Session_component::DEVICE_REMOVE, dev);
				destroy(env()->heap(), dev);
			}
			break;
		}

		case USB_BUS_ADD:
			break;

		case USB_BUS_REMOVE:
			break;
    } 

    return NOTIFY_OK;
}
