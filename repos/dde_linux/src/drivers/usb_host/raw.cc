/*
 * \brief  Server side USB session implementation
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <usb_session/rpc_object.h>
#include <util/list.h>

#include <lx_emul.h>
#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include "raw.h"
#include <legacy/lx_emul/extern_c_end.h>
#include <signal.h>

#include <legacy/lx_kit/malloc.h>
#include <legacy/lx_kit/scheduler.h>

using namespace Genode;

extern "C" int usb_set_configuration(struct usb_device *dev, int configuration);

constexpr bool verbose_raw = false;


namespace Usb {
	class  Session_component;
	class  Root;
	class  Cleaner;
}

/**
 * Keep track of all registered USB devices (via raw driver)
 */
class Device : public List<Device>::Element
{
	using Tx = Usb::Session_rpc_object::Tx;
	using Packet_descriptor = Usb::Packet_descriptor;

	private:

		usb_device &_udev;
		Lx::Task    _task { _run, this, "device_worker",
		                    Lx::Task::PRIORITY_2,
		                    Lx::scheduler() };
		completion  _packet_avail;
		Tx::Sink   *_sink { nullptr };

		Signal_context_capability _sigh_ready;
		unsigned                  _p_in_flight  { 0 };

		template<typename FN>
		void _with_packet_stream(FN const &fn)
		{
			if (_sink == nullptr) return;
			fn(*_sink);
		}

		template <typename FN>
		void _with_interface(unsigned index, FN const &fn)
		{
			if (!_udev.actconfig)
				return;

			if (index >= _udev.actconfig->desc.bNumInterfaces)
				return;

			usb_interface * const interface_ptr = _udev.actconfig->interface[index];

			if (!interface_ptr)
				return;

			fn(*interface_ptr);
		}

		template <typename FN>
		void _for_each_interface(FN const &fn)
		{
			if (!_udev.actconfig)
				return;

			for (unsigned i = 0; i < _udev.actconfig->desc.bNumInterfaces; i++)
				_with_interface(i, fn);
		}

		void _ack_packet(Packet_descriptor p)
		{
			_with_packet_stream([&](Tx::Sink &sink) {
				sink.acknowledge_packet(p);
				_p_in_flight--;
			});
		}

		usb_host_endpoint *_host_ep(uint8_t ep)
		{
			return ep & USB_DIR_IN ? _udev.ep_in[ep & 0xf] : _udev.ep_out[ep & 0xf];
		}

		/**
		 * Retrieve string descriptor at index given in packet
		 */
		void _retrieve_string(Packet_descriptor &p)
		{
			char *buffer = _sink->packet_content(p);
			int   length;

			if ((length = usb_string(&_udev, p.string.index, buffer, p.size())) < 0) {
				warning("Could not read string descriptor index: ", (unsigned)p.string.index);
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

			int err = usb_control_msg(&_udev, usb_rcvctrlpipe(&_udev, 0),
			                          p.control.request, p.control.request_type,
			                          p.control.value, p.control.index, buf,
			                          p.size(), p.control.timeout);

			if (err > 0 && p.size()) {
				_with_packet_stream([&](Tx::Sink &sink) {
					Genode::memcpy(sink.packet_content(p), buf, err);
				});
			}

			kfree(buf);

			if (err >= 0) {
				p.succeded = true;
				p.control.actual_size = err;
			} else {
				p.control.actual_size = 0;

				if (err == -ENOENT)
					p.error = Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				else if ((err == -ENODEV) || (err == -ESHUTDOWN))
					p.error = Packet_descriptor::NO_DEVICE_ERROR;
				else if ((err == -EPROTO) || (err == -EILSEQ))
					p.error = Packet_descriptor::PROTOCOL_ERROR;
				else if (err == -EPIPE)
					p.error = Packet_descriptor::STALL_ERROR;
				else if (err == -ETIMEDOUT)
					p.error = Packet_descriptor::TIMEOUT_ERROR;
				else {
					Genode::error(__func__, ": unhandled error: ", err);
					p.error = Packet_descriptor::UNKNOWN_ERROR;
				}
			}
		}

		/**
		 * Write control transfer
		 */
		void _ctrl_out(Packet_descriptor &p)
		{
			void *buf = kmalloc(4096, GFP_NOIO);

			if (p.size())
				Genode::memcpy(buf, _sink->packet_content(p), p.size());

			int err = usb_control_msg(&_udev, usb_sndctrlpipe(&_udev, 0),
			                          p.control.request, p.control.request_type,
			                          p.control.value, p.control.index, buf, p.size(),
			                          p.control.timeout);

			if (err >= 0) {
				p.succeded = true;
				p.control.actual_size = err;

				if (p.control.request == USB_REQ_CLEAR_FEATURE &&
				    p.control.value == USB_ENDPOINT_HALT) {
					usb_reset_endpoint(&_udev, p.control.index);
				}
			} else {
				p.control.actual_size = 0;

				if (err == -ENOENT)
					p.error = Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				else if ((err == -ENODEV) || (err == -ESHUTDOWN))
					p.error = Packet_descriptor::NO_DEVICE_ERROR;
				else if ((err == -EPROTO) || (err == -EILSEQ))
					p.error = Packet_descriptor::PROTOCOL_ERROR;
				else if (err == -EPIPE)
					p.error = Packet_descriptor::STALL_ERROR;
				else if (err == -ETIMEDOUT)
					p.error = Packet_descriptor::TIMEOUT_ERROR;
				else {
					Genode::error(__func__, ": unhandled error: ", err);
					p.error = Packet_descriptor::UNKNOWN_ERROR;
				}
			}

			kfree(buf);
		}

		/**
		 * Asynchronous transfer helpers
		 */
		struct Complete_data
		{
			Device            *device;
			Packet_descriptor  packet;

			Complete_data(Device *device, Packet_descriptor &p)
			: device(device), packet(p) { }
		};

		Complete_data * alloc_complete_data(Packet_descriptor &p)
		{
			void * data = kmalloc(sizeof(Complete_data), GFP_KERNEL);
			construct_at<Complete_data>(data, this, p);
			return reinterpret_cast<Complete_data *>(data);
		}

		static void free_complete_data(Complete_data *data)
		{
			data->packet.~Packet_descriptor();
			kfree (data);
		}

		void _async_finish(Packet_descriptor &p, urb *urb, bool read)
		{
			if (urb->status == 0) {
				p.transfer.actual_size = urb->actual_length;
				p.succeded             = true;

				if (read) {
					/* make sure the client sees the actual amount of data */
					for (int i = 0; i < urb->number_of_packets; i++) {
						p.transfer.actual_packet_size[i] = urb->iso_frame_desc[i].actual_length;
					}

					/*
					 * We have to copy the whole transfer buffer because the
					 * controller used the offsets into the original buffer to
					 * store the data.
					 */
					_with_packet_stream([&](Tx::Sink &sink) {
						Genode::memcpy(sink.packet_content(p), urb->transfer_buffer,
						               urb->transfer_buffer_length);
					});
				}
			} else if (urb->status == -ESHUTDOWN) {
				p.error = Packet_descriptor::NO_DEVICE_ERROR;
			} else if ((urb->status == -EPROTO) || (urb->status == -EILSEQ)) {
				p.error = Packet_descriptor::PROTOCOL_ERROR;
			} else if (urb->status == -EPIPE) {
				p.error = Packet_descriptor::STALL_ERROR;
			} else {
				Genode::error(__func__, ": unhandled error: ", urb->status);
				p.error = Packet_descriptor::UNKNOWN_ERROR;
			}

			_ack_packet(p);

			if (_sink && _sink->packet_avail())
				packet_avail();
		}

		static void _async_complete(urb *urb)
		{
			Complete_data *data = (Complete_data *)urb->context;

			data->device->_async_finish(data->packet, urb,
			                            !!(data->packet.transfer.ep & USB_DIR_IN));

			free_complete_data(data);
			dma_free(urb->transfer_buffer);
			usb_free_urb(urb);
		}

		/**
		 * Bulk transfer
		 */
		bool _bulk(Packet_descriptor &p, bool read)
		{
			unsigned pipe;
			void    *buf = dma_malloc(p.size());

			if (read)
				pipe = usb_rcvbulkpipe(&_udev, p.transfer.ep);
			else {
				pipe = usb_sndbulkpipe(&_udev, p.transfer.ep);
				Genode::memcpy(buf, _sink->packet_content(p), p.size());
			}

			urb *bulk_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!bulk_urb) {
				error("Failed to allocate bulk URB");
				dma_free(buf);
				p.error = Packet_descriptor::MEMORY_ERROR;
				return false;
			}

			Complete_data *data = alloc_complete_data(p);

			usb_fill_bulk_urb(bulk_urb, &_udev, pipe, buf, p.size(),
			                 _async_complete, data);

			int ret = usb_submit_urb(bulk_urb, GFP_KERNEL);
			if (ret != 0) {
				if (ret == -ENOENT)
					p.error = Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				else if ((ret == -ENODEV) || (ret == -ESHUTDOWN))
					p.error = Packet_descriptor::NO_DEVICE_ERROR;
				else {
					Genode::error(__func__, ": unhandled error: ", ret);
					p.error = Packet_descriptor::UNKNOWN_ERROR;
				}

				free_complete_data(data);
				usb_free_urb(bulk_urb);
				dma_free(buf);
				return false;
			}

			return true;
		}

		/**
		 * IRQ transfer
		 */
		bool _irq(Packet_descriptor &p, bool read)
		{
			unsigned pipe;
			void    *buf = dma_malloc(p.size());

			if (read)
				pipe = usb_rcvintpipe(&_udev, p.transfer.ep);
			else {
				pipe = usb_sndintpipe(&_udev, p.transfer.ep);
				Genode::memcpy(buf, _sink->packet_content(p), p.size());
			}

			urb *irq_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!irq_urb) {
				error("Failed to allocate interrupt URB");
				dma_free(buf);
				p.error = Usb::Packet_descriptor::MEMORY_ERROR;
				return false;
			}

			Complete_data *data = alloc_complete_data(p);

			int polling_interval;

			if (p.transfer.polling_interval == Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL) {

				usb_host_endpoint *ep = _host_ep(p.transfer.ep);

				if (!ep) {
					error("could not get ep: ", p.transfer.ep);
					dma_free(buf);
					p.error = Usb::Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
					return false;
				}

				polling_interval = ep->desc.bInterval;

			} else
				polling_interval = p.transfer.polling_interval;

			usb_fill_int_urb(irq_urb, &_udev, pipe, buf, p.size(),
			                 _async_complete, data, polling_interval);

			int ret = usb_submit_urb(irq_urb, GFP_KERNEL);
			if (ret != 0) {
				if (ret == -ENOENT)
					p.error = Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				else if ((ret == -ENODEV) || (ret == -ESHUTDOWN))
					p.error = Packet_descriptor::NO_DEVICE_ERROR;
				else if (ret == -ENOSPC) {
					/*
					 * ENOSPC (no bandwidth) is handled by the USB HID driver
					 * as return value of submitting an interrupt URB. But
					 * since the USB session delivers the error asynchronously,
					 * the error shows up at the HID driver as late as when
					 * handling the interrupt where this error is not
					 * anticipated.
					 */
					Genode::warning(__func__, ": reflect ENOSPC as STALL_ERROR");
					p.error = Packet_descriptor::STALL_ERROR;
				}
				else {
					Genode::error(__func__, ": unhandled error: ", ret);
					p.error = Packet_descriptor::UNKNOWN_ERROR;
				}

				free_complete_data(data);
				usb_free_urb(irq_urb);
				dma_free(buf);
				return false;
			}

			return true;
		}

		/**
		 * Isochronous transfer
		 */
		bool _isoc(Packet_descriptor &p, bool read)
		{
			unsigned           pipe;
			usb_host_endpoint *ep  = _host_ep(p.transfer.ep);
			void              *buf = dma_malloc(p.size());

			if (read) {
				pipe = usb_rcvisocpipe(&_udev, p.transfer.ep);
			}
			else {
				pipe = usb_sndisocpipe(&_udev, p.transfer.ep);
				Genode::memcpy(buf, _sink->packet_content(p), p.size());
			}

			if (!ep) {
				error("could not get ep: ", p.transfer.ep);
				dma_free(buf);
				p.error = Usb::Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				return false;
			}

			urb *urb = usb_alloc_urb(p.transfer.number_of_packets, GFP_KERNEL);
			if (!urb) {
				error("Failed to allocate isochronous URB");
				dma_free(buf);
				p.error = Usb::Packet_descriptor::MEMORY_ERROR;
				return false;
			}

			Complete_data *data         = alloc_complete_data(p);
			urb->dev                    = &_udev;
			urb->pipe                   = pipe;
			urb->start_frame            = -1;
			urb->stream_id              = 0;
			urb->transfer_buffer        = buf;
			urb->transfer_buffer_length = p.size();
			urb->number_of_packets      = p.transfer.number_of_packets;
			urb->interval               = 1 << min(15, ep->desc.bInterval - 1);
			urb->context                = (void *)data;
			urb->transfer_flags         = URB_ISO_ASAP | (read ? URB_DIR_IN : URB_DIR_OUT);
			urb->complete               = _async_complete;

			unsigned offset = 0;
			for (int i = 0; i < p.transfer.number_of_packets; i++) {
				urb->iso_frame_desc[i].offset = offset;
				urb->iso_frame_desc[i].length = p.transfer.packet_size[i];
				offset += p.transfer.packet_size[i];
			}

			int ret = usb_submit_urb(urb, GFP_KERNEL);
			if (ret == 0)
				return true;

			if (ret == -ENOENT)
				p.error = Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
			else if ((ret == -ENODEV) || (ret == -ESHUTDOWN))
				p.error = Packet_descriptor::NO_DEVICE_ERROR;
			else {
				Genode::error(__func__, ": unhandled error: ", ret);
				p.error = Packet_descriptor::UNKNOWN_ERROR;
			}

			free_complete_data(data);
			usb_free_urb(urb);
			dma_free(buf);
			return false;
		}

		/**
		 * Change alternate settings for device
		 */
		void _alt_setting(Packet_descriptor &p)
		{

			int err = usb_set_interface(&_udev, p.interface.number,
			                            p.interface.alt_setting);

			if (!err)
				p.succeded = true;
			else
				Genode::error(__func__, ": unhandled error: ", err);
		}

		/**
		 * Set configuration
		 */
		void _config(Packet_descriptor &p)
		{
			usb_host_config *config = _udev.actconfig;

			if (config) {
				for (unsigned i = 0; i < config->desc.bNumInterfaces; i++) {
					if (usb_interface_claimed(config->interface[i])) {
						error("There are interfaces claimed, won't set configuration");
						return;
					}
				}
			}

			int err = usb_set_configuration(&_udev, p.number);

			if (!err)
				p.succeded = true;
			else
				Genode::error(__func__, ": unhandled error: ", err);
		}

		/**
		 * Release interface
		 */
		void _release_interface(Packet_descriptor &p)
		{
			usb_interface *iface = interface(p.number);

			if (!iface)
				return;

			usb_driver_release_interface(&raw_intf_driver, iface);
			p.succeded = true;
		}

		/**
		 * Flush all pending URBs for endpoint
		 */
		void _flush_endpoint(Packet_descriptor &p)
		{
			usb_host_endpoint *ep = _host_ep(p.number);

			if (!ep) {
				error("could net get ep: ", p.number);
				p.error = Usb::Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR;
				return;
			}

			usb_hcd_flush_endpoint(&_udev, ep);
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
			while (_sink && _p_in_flight < _sink->ack_slots_free() && _sink->packet_avail()) {
				Packet_descriptor p = _sink->get_packet();

				if (verbose_raw)
					log("PACKET: ", (unsigned)p.type, " first value: ", Hex(p.number));

				_p_in_flight++;

				if (!_sink->packet_valid(p)) {
					p.error = Packet_descriptor::PACKET_INVALID_ERROR;
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
						if (_bulk(p, p.read_transfer()))
							continue;
						break;

					case Packet_descriptor::IRQ:
						if (_irq(p, p.read_transfer()))
							continue;
						break;

					case Packet_descriptor::ISOC:
						if (_isoc(p, p.read_transfer()))
							continue;
						break;

					case Packet_descriptor::ALT_SETTING:
						_alt_setting(p);
						break;

					case Packet_descriptor::CONFIG:
						_config(p);
						break;

					case Packet_descriptor::RELEASE_IF:
						_release_interface(p);
						break;

					case Packet_descriptor::FLUSH_TRANSFERS:
						_flush_endpoint(p);
						break;
				}

				_ack_packet(p);
			}
		}

		/**
		 * Wait for packets
		 */
		void _wait()
		{
			/* wait for device to become ready */
			init_completion(&_packet_avail);
			wait_queue_head_t wait;
			_wait_event(wait, ready());

			report_device_list();

			if (_sigh_ready.valid())
				Signal_transmitter(_sigh_ready).submit();

			while (true) {
				wait_for_completion(&_packet_avail);
				_dispatch();
			}
		}

		static void _run(void *data)
		{
			Device *device = static_cast<Device *>(data);
			device->_wait();
		}

	public:

		Device(usb_device &udev) : _udev(udev)
		{
			list()->insert(this);

			if (!Lx::scheduler().active()) {
				Lx::scheduler().schedule(); }
		}

		~Device()
		{
			Lx::scheduler().remove(&_task);
			list()->remove(this);
			report_device_list();
		}

		void packet_avail() { ::complete(&_packet_avail); }

		unsigned num_interfaces() const
		{
			if (ready())
				return _udev.actconfig->desc.bNumInterfaces;

			return 0;
		}

		void packet_stream(Tx::Sink *sink)
		{
			_sink        = sink;
			_p_in_flight = 0;

			if (!_sink) {
				_sigh_ready = Signal_context_capability { };
			}
		}

		bool ready() const { return _udev.actconfig; }

		void sigh_ready(Signal_context_capability sigh_ready)
		{
			_sigh_ready = sigh_ready;
		}

		static List<Device> *list()
		{
			static List<Device> _l;
			return &_l;
		}

		static Device * device_by_product(uint16_t vendor, uint16_t product)
		{
			for (Device *d = list()->first(); d; d = d->next()) {
				if (d->_udev.descriptor.idVendor == vendor && d->_udev.descriptor.idProduct == product)
					return d;
			}

			return nullptr;
		}

		static Device * device_by_bus(long bus, long dev)
		{
			for (Device *d = list()->first(); d; d = d->next()) {
				if (d->_udev.bus->busnum == bus && d->_udev.devnum == dev)
					return d;
			}

			return nullptr;
		}

		static Device * device_by_class(long class_value, Session_label label)
		{
			for (Device *d = list()->first(); d; d = d->next()) {
				if (class_value ==  d->device_class_value() && label == d->label())
					return d;
			}

			return nullptr;
		}

		static void report_device_list();


		usb_interface *interface(unsigned index)
		{
			if (!_udev.actconfig)
				return nullptr;

			if (index >= _udev.actconfig->desc.bNumInterfaces)
				return nullptr;

			usb_interface *iface = _udev.actconfig->interface[index];
			return iface;
		}


		/**
		 * Return pseudo device class of USB device
		 *
		 * The returned value expresses the type of USB device. If the device
		 * has at least one HID interface, the value is USB_CLASS_HID. Otherwise,
		 * the class of the first interface is interpreted at type the device.
		 *
		 * Note this classification of USB devices is meant as an interim solution
		 * only to assist the implementation of access-control policies.
		 */
		unsigned device_class_value()
		{
			unsigned result = 0;

			_with_interface(0, [&] (usb_interface &interface) {
				if (interface.cur_altsetting)
					result = interface.cur_altsetting->desc.bInterfaceClass; });

			_for_each_interface([&] (usb_interface &interface) {
				if (interface.cur_altsetting)
					if (interface.cur_altsetting->desc.bInterfaceClass == USB_CLASS_HID)
						result = USB_CLASS_HID; });

			return result;
		}

		long bus()  const { return _udev.bus->busnum; }
		long dev()  const { return _udev.devnum; }

		usb_device &udev() const { return _udev; }

		void report(Xml_generator &xml)
		{
			if (!_udev.actconfig)
				return;

			using namespace Genode;
			using Value = String<64>;

			xml.attribute("label",      label());
			xml.attribute("vendor_id",  Value(Hex(_udev.descriptor.idVendor)));
			xml.attribute("product_id", Value(Hex(_udev.descriptor.idProduct)));
			xml.attribute("bus",        Value(Hex(_udev.bus->busnum)));
			xml.attribute("dev",        Value(Hex(_udev.devnum)));
			xml.attribute("class",      Value(Hex(device_class_value())));

			_for_each_interface([&] (usb_interface &interface) {

				if (!interface.cur_altsetting)
					return;

				xml.node("interface", [&] () {

					uint8_t const class_value =
						interface.cur_altsetting->desc.bInterfaceClass;

					uint8_t const protocol_value =
						interface.cur_altsetting->desc.bInterfaceProtocol;

					xml.attribute("class", Value(Hex(class_value)));
					xml.attribute("protocol", Value(Hex(protocol_value)));
				});
			});
		}

		usb_host_endpoint *endpoint(usb_interface *iface, unsigned alt_setting,
		                            unsigned endpoint_num)
		{
			return &iface->altsetting[alt_setting].endpoint[endpoint_num];
		}

		Session_label label()
		{
			if (!_udev.bus)
				return Session_label("usb-unknown");
			return Session_label("usb-", _udev.bus->busnum, "-", _udev.devnum);
		}
};


struct Interface : List<::Interface>::Element
{
	usb_interface *iface;

	Interface(usb_interface *iface) : iface(iface) { }
};


/**
 * Asynchronous USB-interface release
 */
class Usb::Cleaner : List<::Interface>
{
	private:

		static void _run(void *c)
		{
			Cleaner *cleaner = (Cleaner *)c;

			while (true) {
				cleaner->_task.block_and_schedule();

				while (::Interface *interface = cleaner->first()) {
					usb_driver_release_interface(&raw_intf_driver, interface->iface);
					cleaner->remove(interface);
					destroy(Lx::Malloc::mem(), interface);
				}
			}
		}

		Lx::Task _task { _run, this, "raw_cleaner", Lx::Task::PRIORITY_2,
		                 Lx::scheduler() };

	public:

		void schedule_release(usb_interface *iface)
		{
			::Interface *interface = new(Lx::Malloc::mem()) ::Interface(iface);
			insert(interface);
			_task.unblock();

			if (!Lx::scheduler().active())
				Lx::scheduler().schedule();
		}
};


/*****************
 ** USB session **
 *****************/

class Usb::Session_component : public Session_rpc_object,
                               public List<Session_component>::Element
{
	private:

		Session_label                      _label;
		Genode::Entrypoint                &_ep;
		unsigned long                      _vendor;
		unsigned long                      _product;
		long                               _bus = 0;
		long                               _dev = 0;
		long                               _class = 0;
		Device                            *_device = nullptr;
		Signal_context_capability          _sigh_state_change;
		Io_signal_handler<Session_component> _packet_avail;
		Io_signal_handler<Session_component> _ready_ack;
		Ram_dataspace_capability           _tx_ds;
		Usb::Cleaner                      &_cleaner;


		void _drain_packet_stream()
		{
			while (sink()->packet_avail() && sink()->ready_to_ack()) {
				Packet_descriptor p = sink()->get_packet();
				p.succeded = false;
				p.error    = Packet_descriptor::NO_DEVICE_ERROR;
				sink()->acknowledge_packet(p);
			}
		}


		void _signal_state_change()
		{
			if (_sigh_state_change.valid())
				Signal_transmitter(_sigh_state_change).submit(1);
		}

		void _receive()
		{
			if (!_device) {
				_drain_packet_stream();
				return;
			}

			_device->packet_avail();
			if (!Lx::scheduler().active()) {
				Lx::scheduler().schedule();
			}
		}

	public:

		enum State {
			DEVICE_ADD,
			DEVICE_REMOVE,
		};

		Session_component(Session_label label,
		                  Genode::Ram_dataspace_capability tx_ds,
		                  Genode::Entrypoint &ep,
		                  Genode::Region_map &rm,
		                  unsigned long vendor, unsigned long product,
		                  long bus, long dev, long class_,
		                  Usb::Cleaner &cleaner)
		: Session_rpc_object(tx_ds, ep.rpc_ep(), rm),
		  _label(label), _ep(ep), _vendor(vendor), _product(product),
		  _bus(bus), _dev(dev), _class(class_),
		  _packet_avail(ep, *this, &Session_component::_receive),
		  _ready_ack(ep, *this, &Session_component::_receive),
		  _tx_ds(tx_ds), _cleaner(cleaner)
		{
			Device *device_ptr;
			if (bus && dev) {
				device_ptr = Device::device_by_bus(bus, dev);
			} else if (vendor && product) {
				device_ptr = Device::device_by_product(_vendor, _product);
			} else {
				device_ptr = Device::device_by_class(_class, _label);
			}

			if (device_ptr) {
				device_ptr->packet_stream(sink());
				state_change(DEVICE_ADD, device_ptr);
			}

			/* register signal handlers */
			_tx.sigh_packet_avail(_packet_avail);
		}

		~Session_component()
		{
			/* release claimed interfaces */
			if (_device && _device->ready())
				for (unsigned i = 0; i < _device->num_interfaces() ; i++)
					release_interface(i);

			if (_device)
				_device->packet_stream(nullptr);
		}

		/***********************
		 ** Session interface **
		 ***********************/

		bool plugged() { return _device != nullptr; }

		void claim_interface(unsigned interface_num) override
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = _device->interface(interface_num);
			if (!iface)
				throw Interface_not_found();

			if (usb_driver_claim_interface(&raw_intf_driver, iface, nullptr))
				throw Interface_already_claimed();
		}

		void release_interface(unsigned interface_num) override
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = _device->interface(interface_num);
			if (!iface)
				throw Interface_not_found();

			_cleaner.schedule_release(iface);
		}

		void config_descriptor(Device_descriptor *device_descr,
		                       Config_descriptor *config_descr) override
		{
			if (!_device)
				throw Device_not_found();

			Genode::memcpy(device_descr, &_device->udev().descriptor, sizeof(usb_device_descriptor));

			if (_device->udev().actconfig)
				Genode::memcpy(config_descr, &_device->udev().actconfig->desc, sizeof(usb_config_descriptor));
			else
				Genode::memset(config_descr, 0, sizeof(usb_config_descriptor));

			device_descr->num   = _device->udev().devnum;
			device_descr->speed = _device->udev().speed;
		}

		unsigned alt_settings(unsigned index) override
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = _device->interface(index);
			if (!iface)
				throw Interface_not_found();

			return iface->num_altsetting;
		}

		void interface_descriptor(unsigned index, unsigned alt_setting,
		                          Interface_descriptor *interface_descr) override
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = _device->interface(index);
			if (!iface)
				throw Interface_not_found();

			Genode::memcpy(interface_descr, &iface->altsetting[alt_setting].desc,
			               sizeof(usb_interface_descriptor));

			if (&iface->altsetting[alt_setting] == iface->cur_altsetting)
				interface_descr->active = true;
		}

		bool interface_extra(unsigned index, unsigned alt_setting,
		                     Interface_extra *interface_data)
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = _device->interface(index);
			if (!iface)
				throw Interface_not_found();

			Genode::uint8_t length = iface->altsetting[alt_setting].extralen;
			if (length == 0) return false;

			if (length > sizeof(Interface_extra::data))
				length = sizeof(Interface_extra::data);

			Genode::memcpy(interface_data->data, iface->altsetting[alt_setting].extra,
			               length);

			interface_data->length = length;
			return true;
		}

		void endpoint_descriptor(unsigned              interface_num,
		                         unsigned              alt_setting,
		                         unsigned              endpoint_num,
		                         Endpoint_descriptor  *endpoint_descr) override
		{
			if (!_device)
				throw Device_not_found();

			usb_interface *iface = usb_ifnum_to_if(&_device->udev(), interface_num);
			if (!iface)
				throw Interface_not_found();

			Genode::memcpy(endpoint_descr, &_device->endpoint(iface, alt_setting,
			               endpoint_num)->desc, sizeof(usb_endpoint_descriptor));
		}

		/*********************
		 ** Local interface **
		 *********************/

		bool session_device(Device *device)
		{
			usb_device_descriptor *descr = &device->udev().descriptor;
			usb_interface *iface = device->interface(0);

			return (descr->idVendor == _vendor && descr->idProduct == _product)
			       || (_bus && _dev && _bus == device->bus() &&
			           _dev == device->dev())
			       || (iface && iface->cur_altsetting &&
			           _class == device->device_class_value() &&
			           _label == device->label())? true : false;
		}

		bool state_change(State state, Device *device)
		{
			switch (state) {
				case DEVICE_ADD:
					if (!session_device(device))
						return false;

					if (_device)
						warning("Device type already present (vendor: ",
						         Hex(device->udev().descriptor.idVendor),
						         " product: ", Hex(device->udev().descriptor.idProduct),
						         ") Overwrite!");

					_device = device;
					_device->packet_stream(sink());
					_device->sigh_ready(_sigh_state_change);
					return true;

				case DEVICE_REMOVE:
					if (device != _device)
						return false;

					_device = nullptr;
					_signal_state_change();
					return true;
			}

			return false;
		}

		void sigh_state_change(Signal_context_capability sigh)
		{
			_sigh_state_change = sigh;

			if (_device && _device->ready()) {
				Signal_transmitter(_sigh_state_change).submit();
			} else if (_device)
				_device->sigh_ready(sigh);
		}

		Ram_dataspace_capability tx_ds() { return _tx_ds; }
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

		Genode::Env        &_env;

		Genode::Signal_handler<Usb::Root> _config_handler = {
			_env.ep(), *this, &Usb::Root::_handle_config };

		Genode::Reporter _config_reporter { _env, "config" };

		Genode::Reporter _device_list_reporter {
			_env, "devices", "devices", 512*1024 };

		Usb::Cleaner     _cleaner;

		void _handle_config()
		{
			Lx_kit::env().config_rom().update();

			Genode::Xml_node config = Lx_kit::env().config_rom().xml();

			if (!_config_reporter.enabled())
				_config_reporter.enabled(true);

			bool const uhci = config.attribute_value<bool>("uhci", false);
			bool const ehci = config.attribute_value<bool>("ehci", false);
			bool const xhci = config.attribute_value<bool>("xhci", false);
			bool const ohci = config.attribute_value<bool>("ohci", false);

			Genode::Reporter::Xml_generator xml(_config_reporter, [&] {
				if (uhci) xml.attribute("uhci", "yes");
				if (ehci) xml.attribute("ehci", "yes");
				if (xhci) xml.attribute("xhci", "yes");
				if (ohci) xml.attribute("ohci", "yes");

				config.with_raw_content([&] (char const *start, size_t length) {
					xml.append(start, length); });
			});
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;
			using Genode::size_t;

			Session_label const label = label_from_args(args);
			try {
				Xml_node config_node = Lx_kit::env().config_rom().xml();
				Genode::Session_policy policy(label, config_node);

				size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				unsigned long vendor  = policy.attribute_value<unsigned long>("vendor_id", 0);
				unsigned long product = policy.attribute_value<unsigned long>("product_id", 0);
				unsigned long bus     = policy.attribute_value<unsigned long>("bus", 0);
				unsigned long dev     = policy.attribute_value<unsigned long>("dev", 0);
				unsigned long class_  = policy.attribute_value<unsigned long>("class", 0);

				if (tx_buf_size > ram_quota) {
					warning("Insufficient RAM quota, got ", ram_quota, " need ",
					        tx_buf_size);
					throw Insufficient_ram_quota();
				}

				Ram_dataspace_capability tx_ds = _env.ram().alloc(tx_buf_size);
				Session_component *session = new (md_alloc())
					Session_component(label.last_element(), tx_ds, _env.ep(),
					                  _env.rm(), vendor, product, bus, dev,
					                  class_, _cleaner);
				::Session::list()->insert(session);
				return session;
			}
			catch (Genode::Session_policy::No_policy_defined) {
				error("Invalid session request, no matching policy for '",
				      label.string(), "'");
				throw Genode::Service_denied();
			}
		}

		void _destroy_session(Session_component *session)
		{
			Ram_dataspace_capability tx_ds = session->tx_ds();

			::Session::list()->remove(session);
			Genode::Root_component<Session_component>::_destroy_session(session);

			_env.ram().free(tx_ds);
		}

	public:

		Root(Genode::Env &env,
		     Genode::Allocator &md_alloc,
		     bool report_device_list)
		: Genode::Root_component<Session_component>(env.ep(), md_alloc),
			_env(env)
		{
			Lx_kit::env().config_rom().sigh(_config_handler);
			_device_list_reporter.enabled(report_device_list);
		}

		Genode::Reporter &device_list_reporter()
		{
			return _device_list_reporter;
		}
};


static Genode::Constructible<Usb::Root> root;


void Raw::init(Genode::Env &env, bool report_device_list)
{
	root.construct(env, Lx::Malloc::mem(), report_device_list);
	env.parent().announce(env.ep().manage(*root));
}


void Device::report_device_list()
{
	if (!root->device_list_reporter().enabled())
		return;

	Genode::Reporter::Xml_generator xml(root->device_list_reporter(), [&] ()
	{

		for (Device *d = list()->first(); d; d = d->next()) {

			if (!d->udev().bus) {
				Genode::warning("device ", d->label().string(), " state incomplete");
				continue;
			}

			xml.node("device", [&] () { d->report(xml); });
		}
	});
}

/*****************
 ** C interface **
 *****************/

int raw_notify(struct notifier_block *nb, unsigned long action, void *data)
{
	struct usb_device *udev = (struct usb_device*)data;

	if (verbose_raw)
		log("RAW: ",action == USB_DEVICE_ADD ? "Add" : "Remove",
		   " vendor: ",  Hex(udev->descriptor.idVendor),
		   " product: ", Hex(udev->descriptor.idProduct));

	switch (action) {

		case USB_DEVICE_ADD:
		{
			::Session::list()->state_change(Usb::Session_component::DEVICE_ADD,
			                                new (Lx::Malloc::mem()) Device(*udev));
			break;
		}

		case USB_DEVICE_REMOVE:
		{
			Device *dev = Device::device_by_bus(udev->bus->busnum, udev->devnum);
			if (dev) {
				::Session::list()->state_change(Usb::Session_component::DEVICE_REMOVE, dev);
				destroy(Lx::Malloc::mem(), dev);
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
