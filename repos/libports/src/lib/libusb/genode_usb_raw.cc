/*
 * \brief  Genode backend for libusb
 * \author Christian Prochaska
 * \author Stefan Kalkowski
 * \date   2016-09-19
 */

/*
 * Copyright (C) 2016-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/registry.h>
#include <base/allocator_avl.h>
#include <base/signal.h>
#include <base/tslab.h>
#include <usb_session/device.h>

#include <fcntl.h>
#include <time.h>
#include <libc/allocator.h>

#include <internal/thread_create.h>

#include "libusbi.h"

using namespace Genode;

bool libusb_genode_backend_signaling = false;
static int vfs_libusb_fd { -1 };


struct Usb_device
{
	template <typename URB>
	struct Urb_tpl : URB
	{
		void          * const buf;
		size_t          const size;
		usbi_transfer * const itransfer { nullptr };

		template <typename... ARGS>
		Urb_tpl(void *buf, size_t size, usbi_transfer *itransfer,
		        ARGS &&... args)
		: URB(args...), buf(buf), size(size), itransfer(itransfer) {}

		template <typename... ARGS>
		Urb_tpl(void *buf, size_t size, ARGS &&... args)
		: URB(args...), buf(buf), size(size) {}
	};

	struct Interface : Usb::Interface, Registry<Usb_device::Interface>::Element
	{
		using Urb = Urb_tpl<Usb::Interface::Urb>;

		Usb_device &_device;

		Interface(Usb_device &device, uint8_t idx);

		void handle_events();
	};


	using Urb = Urb_tpl<Usb::Device::Urb>;

	Env                        &_env;
	Allocator                  &_alloc;
	Tslab<Interface::Urb, 4096> _iface_slab { _alloc };
	Signal_context_capability   _handler_cap;
	Usb::Connection             _session { _env };
	Usb::Device                 _device  { _session, _alloc, _env.rm() };
	libusb_speed                _speed { LIBUSB_SPEED_UNKNOWN };
	unsigned                    _open { 0 };
	Registry<Interface>         _interfaces {};


	void _wait_for_urb(Urb &urb);

	Usb_device(Env &env, Allocator &alloc, Signal_context_capability cap)
	:
		_env(env), _alloc(alloc), _handler_cap(cap)
	{
		String<32> speed;
		_session.with_xml([&] (Xml_node xml) {
			xml.with_optional_sub_node("device", [&] (Xml_node node) {
				speed = node.attribute_value("speed", String<32>()); });
		});

		if      (speed == "low")            _speed = LIBUSB_SPEED_LOW;
		else if (speed == "full")           _speed = LIBUSB_SPEED_FULL;
		else if (speed == "high")           _speed = LIBUSB_SPEED_HIGH;
		else if (speed == "super" ||
				 speed == "super_plus" ||
		         speed == "super_plus_2x2") _speed = LIBUSB_SPEED_SUPER;

		_device.sigh(_handler_cap);
	}

	void close() { _open--; }
	void open()  { _open++; }

	void handle_events();

	static Constructible<Usb_device>& singleton();
};


Usb_device::Interface::Interface(Usb_device &device, uint8_t idx)
:
	Usb::Interface(device._device, Usb::Interface::Index{idx, 0}, (1UL << 20)),
	Registry<Usb_device::Interface>::Element(device._interfaces, *this),
	_device(device)
{
	sigh(device._handler_cap);
}


static inline addr_t isoc_packet_offset(uint32_t idx,
                                        struct libusb_transfer *transfer)
{
	addr_t offset = 0;
	for (uint32_t i = 0; i < idx; i++)
		offset += transfer->iso_packet_desc[i].length;
	return offset;
}


void Usb_device::Interface::handle_events()
{
	update_urbs<Urb>(

		/* produce out content */
		[] (Urb &urb, Byte_range_ptr &dst) {
			memcpy(dst.start, urb.buf, min(dst.num_bytes, urb.size)); },

		/* consume in results */
		[] (Urb &urb, Const_byte_range_ptr const &src) {
			memcpy(urb.buf, src.start, min(src.num_bytes, urb.size));
			if (urb.itransfer) urb.itransfer->transferred = src.num_bytes;
		},

		/* produce out content of isochronous packet i */
		[] (Urb &urb, uint32_t i, Byte_range_ptr &dst) {
			struct libusb_transfer *transfer =
				USBI_TRANSFER_TO_LIBUSB_TRANSFER(urb.itransfer);
			if (IS_XFEROUT(transfer)) {
				addr_t src = (addr_t)urb.buf+isoc_packet_offset(i, transfer);
				memcpy(dst.start, (void*)src, min(transfer->iso_packet_desc[i].length,
				                                  dst.num_bytes));
			}
			return transfer->iso_packet_desc[i].length;
		},

		/* consume in results of isochronous packet i */
		[] (Urb &urb, uint32_t i, Const_byte_range_ptr const &src) {
			struct libusb_transfer *transfer =
				USBI_TRANSFER_TO_LIBUSB_TRANSFER(urb.itransfer);
			addr_t dst = (addr_t)urb.buf+isoc_packet_offset(i, transfer);
			memcpy((void*)dst, src.start, src.num_bytes);
			transfer->iso_packet_desc[i].actual_length = src.num_bytes;
			transfer->iso_packet_desc[i].status = LIBUSB_TRANSFER_COMPLETED;
		},

		/* complete USB request */
		[this] (Urb &urb, Usb::Tagged_packet::Return_value v)
		{
			if (v != Usb::Tagged_packet::OK)
				error("transfer failed, return value ", (int)v);

			if (!urb.itransfer)
				return;

			libusb_context *ctx = _device._open ? ITRANSFER_CTX(urb.itransfer)
			                                    : nullptr;
			usbi_signal_transfer_completion(urb.itransfer);
			if (ctx) usbi_signal_event(ctx);
			destroy(_device._iface_slab, &urb);
		});
}


void Usb_device::_wait_for_urb(Urb &urb)
{
	while (!urb.completed()) {
		handle_events();

		struct pollfd pollfds {
			.fd      = vfs_libusb_fd,
			.events  = POLLIN,
			.revents = 0
		};

		int poll_result = poll(&pollfds, 1, -1);
		if ((poll_result != 1) || !(pollfds.revents & POLLIN))
			error("could not complete request");
	}
}


void Usb_device::handle_events()
{
	_device.update_urbs<Urb>(

		/* produce out content */
		[] (Urb &urb, Byte_range_ptr &dst) {
			memcpy(dst.start, urb.buf, min(dst.num_bytes, urb.size)); },

		/* consume in results */
		[] (Urb &urb, Const_byte_range_ptr const &src) {
			memcpy(urb.buf, src.start, min(src.num_bytes, urb.size));
			if (urb.itransfer) urb.itransfer->transferred = src.num_bytes;
		},

		/* complete USB request */
		[this] (Urb &urb, Usb::Tagged_packet::Return_value v)
		{
			if (v != Usb::Tagged_packet::OK)
				error("control transfer failed, return value ", (int)v);

			if (!urb.itransfer)
				return;

			libusb_context *ctx = _open ? ITRANSFER_CTX(urb.itransfer)
			                            : nullptr;
			usbi_signal_transfer_completion(urb.itransfer);
			if (ctx) usbi_signal_event(ctx);
			destroy(_alloc, &urb);
		});
}


Constructible<Usb_device>& Usb_device::singleton()
{
	static Constructible<Usb_device> dev {};
	return dev;
}


static Usb_device& device()
{
	struct Libusb_not_initialized {};

	if (!Usb_device::singleton().constructed())
		throw Libusb_not_initialized();

	return *Usb_device::singleton();
}


void libusb_genode_backend_init(Env &env, Allocator &alloc,
                                Signal_context_capability handler)
{
	Usb_device::singleton().construct(env, alloc, handler);
}


static int genode_init(struct libusb_context* ctx)
{
	if (vfs_libusb_fd == -1) {
		vfs_libusb_fd = open("/dev/libusb", O_RDONLY);
		if (vfs_libusb_fd == -1) {
			Genode::error("could not open /dev/libusb");
			return LIBUSB_ERROR_OTHER;
		}
	} else {
		Genode::error("tried to init genode usb context twice");
		return LIBUSB_ERROR_OTHER;
	}

	return LIBUSB_SUCCESS;
}


static void genode_exit(void)
{
	Usb_device::singleton().constructed();
		Usb_device::singleton().destruct();

	if (vfs_libusb_fd != -1) {
		close(vfs_libusb_fd);
		vfs_libusb_fd = -1;
	}
}


int genode_get_device_list(struct libusb_context *ctx,
                           struct discovered_devs **discdevs)
{
	unsigned long session_id;
	struct libusb_device *dev;

	uint8_t busnum = 1;
	uint8_t devaddr = 1;

	session_id = busnum << 8 | devaddr;
	usbi_dbg("busnum %d devaddr %d session_id %ld", busnum, devaddr,
		session_id);

	dev = usbi_get_device_by_session_id(ctx, session_id);

	if (!dev) {
	
		usbi_dbg("allocating new device for %d/%d (session %ld)",
		 	 busnum, devaddr, session_id);
		dev = usbi_alloc_device(ctx, session_id);
		if (!dev)
			return LIBUSB_ERROR_NO_MEM;

		/* initialize device structure */
		dev->bus_number = busnum;
		dev->device_address = devaddr;
		dev->speed = device()._speed;

		int result = usbi_sanitize_device(dev);
		if (result < 0) {
			libusb_unref_device(dev);
			return result;
		}

	} else {
		usbi_dbg("session_id %ld already exists", session_id);
	}

	if (discovered_devs_append(*discdevs, dev) == NULL) {
		libusb_unref_device(dev);
		return LIBUSB_ERROR_NO_MEM;
	}

	libusb_unref_device(dev);

	return LIBUSB_SUCCESS;
}


static int genode_open(struct libusb_device_handle *dev_handle)
{
	device().open();
	return usbi_add_pollfd(HANDLE_CTX(dev_handle), vfs_libusb_fd,
	                       POLLIN);
}


static void genode_close(struct libusb_device_handle *dev_handle)
{
	device().close();
	usbi_remove_pollfd(HANDLE_CTX(dev_handle), vfs_libusb_fd);
}


static int genode_get_device_descriptor(struct libusb_device *,
                                        unsigned char* buffer,
                                        int *host_endian)
{
	Usb_device::Urb urb(buffer, sizeof(libusb_device_descriptor),
	                    device()._device, LIBUSB_REQUEST_GET_DESCRIPTOR,
	                    LIBUSB_ENDPOINT_IN, (LIBUSB_DT_DEVICE << 8) | 0, 0,
	                    LIBUSB_DT_DEVICE_SIZE);
	device()._wait_for_urb(urb);
	*host_endian = 0;
	return LIBUSB_SUCCESS;
}


static int genode_get_config_descriptor(struct libusb_device *,
                                        uint8_t idx,
                                        unsigned char *buffer,
                                        size_t len,
                                        int *host_endian)
{
	/* read minimal config descriptor */
	genode_usb_config_descriptor desc;
	Usb_device::Urb cfg(&desc, sizeof(desc), device()._device,
	                    LIBUSB_REQUEST_GET_DESCRIPTOR, LIBUSB_ENDPOINT_IN,
	                    (LIBUSB_DT_CONFIG << 8) | idx, 0, sizeof(desc));
	device()._wait_for_urb(cfg);

	/* read again whole configuration */
	Usb_device::Urb all(buffer, len, device()._device,
	                    LIBUSB_REQUEST_GET_DESCRIPTOR, LIBUSB_ENDPOINT_IN,
	                    (LIBUSB_DT_CONFIG << 8) | idx, 0,
	                    (size_t)desc.total_length);
	device()._wait_for_urb(all);

	*host_endian = 0;
	return desc.total_length;
}


static int genode_get_active_config_descriptor(struct libusb_device *device,
                                               unsigned char *buffer,
                                               size_t len,
                                               int *host_endian)
{
	return genode_get_config_descriptor(device, 0, buffer, len, host_endian);
}


static int genode_set_configuration(struct libusb_device_handle *dev_handle,
                                    int config)
{
	Genode::error(__PRETTY_FUNCTION__,
	              ": not implemented (return address: ",
	              Genode::Hex((Genode::addr_t)__builtin_return_address(0)),
	              ") \n");
	return LIBUSB_ERROR_NOT_SUPPORTED;
}


static int genode_claim_interface(struct libusb_device_handle *dev_handle,
                                  int interface_number)
{
	bool found = false;

	device()._interfaces.for_each([&] (Usb_device::Interface const &iface) {
		if (iface.index().number == interface_number) found = true; });

	if (found) {
		error(__PRETTY_FUNCTION__, ": interface already claimed");
		return LIBUSB_ERROR_BUSY;
	}

	if (interface_number > 0xff || interface_number < 0) {
		error(__PRETTY_FUNCTION__, ": invalid interface number ",
		      interface_number);
		return LIBUSB_ERROR_OTHER;
	}

	new (device()._alloc)
		Usb_device::Interface(device(), (uint8_t) interface_number);
	return LIBUSB_SUCCESS;
}


static int genode_release_interface(struct libusb_device_handle *dev_handle,
                                    int interface_number)
{
	int ret = LIBUSB_ERROR_NOT_FOUND;

	device()._interfaces.for_each([&] (Usb_device::Interface &iface) {
		if (iface.index().number != interface_number)
			return;
		destroy(device()._alloc, &iface);
		ret = LIBUSB_SUCCESS;
	});

	return ret;
}


static int genode_set_interface_altsetting(struct libusb_device_handle* dev_handle,
                                           int interface_number,
                                           int altsetting)
{
	using P  = Usb::Device::Packet_descriptor;
	using Rt = P::Request_type;

	if (interface_number < 0 || interface_number > 0xff ||
	    altsetting < 0 || altsetting > 0xff)
		return LIBUSB_ERROR_INVALID_PARAM;

	Usb_device::Urb urb(nullptr, 0, device()._device, P::Request::SET_INTERFACE,
	                    Rt::value(P::Recipient::IFACE, P::Type::STANDARD,
	                              P::Direction::OUT),
	                    (uint8_t)altsetting, (uint8_t)interface_number, 0);
	device()._wait_for_urb(urb);
	return LIBUSB_SUCCESS;
}


static int genode_submit_transfer(struct usbi_transfer * itransfer)
{
	struct libusb_transfer *transfer =
		USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	Usb::Interface::Packet_descriptor::Type type =
		Usb::Interface::Packet_descriptor::FLUSH;

	switch (transfer->type) {

		case LIBUSB_TRANSFER_TYPE_CONTROL: {

			struct libusb_control_setup *setup =
				(struct libusb_control_setup*)transfer->buffer;

			void * addr = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;
			new (device()._alloc)
				Usb_device::Urb(addr, setup->wLength, itransfer,
				                device()._device, setup->bRequest,
				                setup->bmRequestType, setup->wValue,
				                setup->wIndex, setup->wLength);
			device().handle_events();
			return LIBUSB_SUCCESS;
		}

		case LIBUSB_TRANSFER_TYPE_BULK:
		case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
			type = Usb::Interface::Packet_descriptor::BULK;
			break;
		case LIBUSB_TRANSFER_TYPE_INTERRUPT:
			type = Usb::Interface::Packet_descriptor::IRQ;
			break;
		case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
			type  = Usb::Interface::Packet_descriptor::ISOC;
			break;

		default:
			usbi_err(TRANSFER_CTX(transfer),
				"unknown endpoint type %d", transfer->type);
			return LIBUSB_ERROR_INVALID_PARAM;
	}

	bool found = false;
	device()._interfaces.for_each([&] (Usb_device::Interface &iface) {
		iface.for_each_endpoint([&] (Usb::Endpoint &ep) {
			if (found || transfer->endpoint != ep.address())
				return;
			found = true;
			new (device()._iface_slab)
				Usb_device::Interface::Urb(transfer->buffer, transfer->length,
				                           itransfer, iface, ep, type,
				                           transfer->length,
				                           transfer->num_iso_packets);
			iface.handle_events();
		});
	});

	return found ? LIBUSB_SUCCESS : LIBUSB_ERROR_NOT_FOUND;
}


static int genode_cancel_transfer(struct usbi_transfer * itransfer)
{
	return LIBUSB_SUCCESS;
}


static void genode_clear_transfer_priv(struct usbi_transfer * itransfer) { }


static int genode_handle_events(struct libusb_context *, struct pollfd *,
                                POLL_NFDS_TYPE, int)
{
	libusb_genode_backend_signaling = false;
	device().handle_events();
	device()._interfaces.for_each([&] (Usb_device::Interface &iface) {
		iface.handle_events(); });
	return LIBUSB_SUCCESS;
}


static int genode_handle_transfer_completion(struct usbi_transfer * itransfer)
{
	enum libusb_transfer_status status = LIBUSB_TRANSFER_COMPLETED;

	if (itransfer->flags & USBI_TRANSFER_CANCELLING)
		status = LIBUSB_TRANSFER_CANCELLED;

	return usbi_handle_transfer_completion(itransfer, status);
}


static int genode_clock_gettime(int clkid, struct timespec *tp)
{
	switch (clkid) {
		case USBI_CLOCK_MONOTONIC:
			return clock_gettime(CLOCK_MONOTONIC, tp);
		case USBI_CLOCK_REALTIME:
			return clock_gettime(CLOCK_REALTIME, tp);
		default:
			return LIBUSB_ERROR_INVALID_PARAM;
	}
}


const struct usbi_os_backend genode_usb_raw_backend = {
	/*.name =*/ "Genode",
	/*.caps =*/ 0,
	/*.init =*/ genode_init,
	/*.exit =*/ genode_exit,
	/*.get_device_list =*/ genode_get_device_list,
	/*.hotplug_poll =*/ NULL,
	/*.open =*/ genode_open,
	/*.close =*/ genode_close,
	/*.get_device_descriptor =*/ genode_get_device_descriptor,
	/*.get_active_config_descriptor =*/ genode_get_active_config_descriptor,
	/*.get_config_descriptor =*/ genode_get_config_descriptor,
	/*.get_config_descriptor_by_value =*/ NULL,


	/*.get_configuration =*/ NULL,
	/*.set_configuration =*/ genode_set_configuration,
	/*.claim_interface =*/ genode_claim_interface,
	/*.release_interface =*/ genode_release_interface,

	/*.set_interface_altsetting =*/ genode_set_interface_altsetting,
	/*.clear_halt =*/ NULL,
	/*.reset_device =*/ NULL,

	/*.alloc_streams =*/ NULL,
	/*.free_streams =*/ NULL,

	/*.kernel_driver_active =*/ NULL,
	/*.detach_kernel_driver =*/ NULL,
	/*.attach_kernel_driver =*/ NULL,

	/*.destroy_device =*/ NULL,

	/*.submit_transfer =*/ genode_submit_transfer,
	/*.cancel_transfer =*/ genode_cancel_transfer,
	/*.clear_transfer_priv =*/ genode_clear_transfer_priv,

	/*.handle_events =*/ genode_handle_events,
	/*.handle_transfer_completion =*/ genode_handle_transfer_completion,

	/*.clock_gettime =*/ genode_clock_gettime,

#ifdef USBI_TIMERFD_AVAILABLE
	/*.get_timerfd_clockid =*/ NULL,
#endif

	/*.device_priv_size =*/ 0,
	/*.device_handle_priv_size =*/ 0,
	/*.transfer_priv_size =*/ 0,
};
