/*
 * \brief  Genode backend for libusb
 * \author Christian Prochaska
 * \date   2016-09-19
 */

/*
 * Copyright (C) 2016-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/allocator_avl.h>
#include <base/signal.h>
#include <usb/usb.h>
#include <usb_session/connection.h>

#include <fcntl.h>
#include <time.h>
#include <libc/allocator.h>

#include <internal/thread_create.h>

#include "libusbi.h"


static Libc::Allocator libc_alloc { };


struct Completion : Usb::Completion
{
	struct usbi_transfer *itransfer;

	Completion(struct usbi_transfer *itransfer)
	: itransfer(itransfer) { }

	void complete(Usb::Packet_descriptor &p) override { }
};


struct Usb_device
{
	private:

		unsigned _open { 0 };

	public:

		struct Device_has_vanished {};

		Usb::Connection *usb_connection;
		int              vfs_libusb_fd;

		Usb::Device_descriptor  device_descriptor;
		Usb::Config_descriptor  config_descriptor;
		char                   *raw_config_descriptor = nullptr;

		bool _retrieve_raw_config_descriptor()
		{
			Genode::log("libusb: retrieve configuration descriptor");

			Usb::Packet_descriptor p =
				usb_connection->alloc_packet(config_descriptor.total_length);

			p.type                 = Usb::Packet_descriptor::CTRL;
			p.control.request      = LIBUSB_REQUEST_GET_DESCRIPTOR;
			p.control.request_type = LIBUSB_ENDPOINT_IN;
			p.control.value        = (LIBUSB_DT_CONFIG << 8) | 0;
			p.control.index        = 0;

			usb_connection->source()->submit_packet(p);

			/* wait for ack */

			struct pollfd pollfds {
				.fd = vfs_libusb_fd,
				.events = POLLIN,
				.revents = 0
			};

			int poll_result = poll(&pollfds, 1, -1);

			if ((poll_result != 1) || !(pollfds.revents & POLLIN)) {
				Genode::error(__PRETTY_FUNCTION__,
				              ": could not read raw configuration descriptor");
				return false;
			}

			p = usb_connection->source()->get_acked_packet();

			bool ret = false;
			if (!p.succeded) {
				Genode::error(__PRETTY_FUNCTION__,
				              ": could not read raw configuration descriptor");
			} else if (p.control.actual_size != config_descriptor.total_length) {
				Genode::error(__PRETTY_FUNCTION__,
				              ": received configuration descriptor of unexpected size");
			} else {
				char *packet_content = usb_connection->source()->packet_content(p);
				Genode::memcpy(raw_config_descriptor, packet_content,
				               config_descriptor.total_length);
				ret = true;
			}

			usb_connection->source()->release_packet(p);
			return ret;
		}

		Usb_device(Usb::Connection *usb, int vfs_libusb_fd)
		: usb_connection(usb), vfs_libusb_fd(vfs_libusb_fd)
		{
			usb_connection->config_descriptor(&device_descriptor, &config_descriptor);

			raw_config_descriptor = (char*)malloc(config_descriptor.total_length);

			for (unsigned attempt = 0; attempt < 10; ++attempt)
				if (_retrieve_raw_config_descriptor())
					break;
		}

		~Usb_device()
		{
			free(raw_config_descriptor);
		}

		bool altsetting(int number, int alt_setting)
		{
			if (!usb_connection->source()->ready_to_submit())
				return false;

			Usb::Packet_descriptor p =
				usb_connection->alloc_packet(0);

			p.type                  = Usb::Packet_descriptor::ALT_SETTING;
			p.interface.number      = number;
			p.interface.alt_setting = alt_setting;

			usb_connection->source()->submit_packet(p);

			return true;
		}

		void close() { _open--; }
		void open()  { _open++; }

		void handle_events()
		{
			struct libusb_context *ctx = nullptr;

			while (usb_connection->source()->ack_avail()) {

				Usb::Packet_descriptor p =
					usb_connection->source()->get_acked_packet();

				if (p.type == Usb::Packet_descriptor::ALT_SETTING) {
					usb_connection->source()->release_packet(p);
					continue;
				}

				Completion *completion = static_cast<Completion*>(p.completion);
				struct usbi_transfer *itransfer = completion->itransfer;

				if (_open)
					ctx = ITRANSFER_CTX(itransfer);

				destroy(libc_alloc, completion);

				if (_open == 0) {
					usb_connection->source()->release_packet(p);
					continue;
				}

				if (!p.succeded || itransfer->flags & USBI_TRANSFER_CANCELLING) {
					if (!p.succeded) {
						if (p.error == Usb::Packet_descriptor::NO_DEVICE_ERROR)
							throw Device_has_vanished();
					}
					itransfer->transferred = 0;
					usb_connection->source()->release_packet(p);
					usbi_signal_transfer_completion(itransfer);
					continue;
				}

				char *packet_content = usb_connection->source()->packet_content(p);

				struct libusb_transfer *transfer =
					USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

				switch (transfer->type) {

					case LIBUSB_TRANSFER_TYPE_CONTROL: {

						itransfer->transferred = p.control.actual_size;

						struct libusb_control_setup *setup =
							(struct libusb_control_setup*)transfer->buffer;

						if ((setup->bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) ==
						    LIBUSB_ENDPOINT_IN) {
							Genode::memcpy(transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE,
							               packet_content, p.control.actual_size);
						}

						break;
					}

					case LIBUSB_TRANSFER_TYPE_BULK:
					case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
					case LIBUSB_TRANSFER_TYPE_INTERRUPT: {

						itransfer->transferred = p.transfer.actual_size;

						if (IS_XFERIN(transfer))
							Genode::memcpy(transfer->buffer, packet_content,
							               p.transfer.actual_size);

						break;
					}

					case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: {

						itransfer->transferred = p.transfer.actual_size;

						if (IS_XFERIN(transfer)) {

							Usb::Isoc_transfer &isoc = *(Usb::Isoc_transfer *)packet_content;

							unsigned out_offset = 0;
							for (unsigned i = 0; i < isoc.number_of_packets; i++) {
								size_t const actual_length = isoc.actual_packet_size[i];

								/*
								 * Copy the data from the proper offsets within the buffer as
								 * a short read is still stored at this location.
								 */
								unsigned char       * dst = transfer->buffer + out_offset;
								         char const * src = isoc.data() + out_offset;

								Genode::memcpy(dst, src, actual_length);
								out_offset += transfer->iso_packet_desc[i].length;

								transfer->iso_packet_desc[i].actual_length = actual_length;
								transfer->iso_packet_desc[i].status = LIBUSB_TRANSFER_COMPLETED;
							}
							transfer->num_iso_packets = isoc.number_of_packets;

						}

						break;
					}

					default:
						Genode::error(__PRETTY_FUNCTION__,
						              ": unsupported transfer type");
						usb_connection->source()->release_packet(p);
						continue;
				}

				usb_connection->source()->release_packet(p);

				usbi_signal_transfer_completion(itransfer);
			}

			if (ctx != nullptr)
				usbi_signal_event(ctx);
		}
};


static int vfs_libusb_fd { -1 };
static Usb::Connection *usb_connection { nullptr };
static Usb_device *device_instance { nullptr };


/*
 * This function is called by the VFS plugin on 'open()'
 * to pass a pointer to the USB connection.
 */
void libusb_genode_usb_connection(Usb::Connection *usb)
{
	usb_connection = usb;
}


static int genode_init(struct libusb_context* ctx)
{
	if (!device_instance) {
		vfs_libusb_fd = open("/dev/libusb", O_RDONLY);
		if (vfs_libusb_fd == -1) {
			Genode::error("could not open /dev/libusb");
			return LIBUSB_ERROR_OTHER;
		}
		device_instance = new (libc_alloc) Usb_device(usb_connection,
		                                              vfs_libusb_fd);
	} else {
		Genode::error("tried to init genode usb context twice");
		return LIBUSB_ERROR_OTHER;
	}

	return LIBUSB_SUCCESS;
}


static void genode_exit(void)
{
	if (device_instance) {
		destroy(libc_alloc, device_instance);
		device_instance = nullptr;
		close(vfs_libusb_fd);
		usb_connection = nullptr;
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

		Usb_device *usb_device = device_instance;
		*(Usb_device**)dev->os_priv = usb_device;

		switch (usb_device->device_descriptor.speed) {
			case Usb::Device::SPEED_LOW:
				dev->speed = LIBUSB_SPEED_LOW;
				break;
			case Usb::Device::SPEED_FULL:
				dev->speed = LIBUSB_SPEED_FULL;
				break;
			case Usb::Device::SPEED_HIGH:
				dev->speed = LIBUSB_SPEED_HIGH;
				break;
			case Usb::Device::SPEED_SUPER:
				dev->speed = LIBUSB_SPEED_SUPER;
				break;
			default:
				Genode::warning(__PRETTY_FUNCTION__, ": unknown device speed");
				dev->speed = LIBUSB_SPEED_UNKNOWN;
		}

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
	if (device_instance)
		device_instance->open();

	return usbi_add_pollfd(HANDLE_CTX(dev_handle), vfs_libusb_fd,
	                       POLLIN);
}


static void genode_close(struct libusb_device_handle *dev_handle)
{
	if (device_instance)
		device_instance->close();

	usbi_remove_pollfd(HANDLE_CTX(dev_handle), vfs_libusb_fd);
}


static int genode_get_device_descriptor(struct libusb_device *device,
                                        unsigned char* buffer,
                                        int *host_endian)
{
	Usb_device *usb_device = *(Usb_device**)device->os_priv;

	Genode::memcpy(buffer, &usb_device->device_descriptor,
	               sizeof(libusb_device_descriptor));

	*host_endian = 0;

	return LIBUSB_SUCCESS;
}


static int genode_get_config_descriptor(struct libusb_device *device,
                                        uint8_t config_index,
                                        unsigned char *buffer,
                                        size_t len,
                                        int *host_endian)
{
	if (config_index != 0) {
		Genode::error(__PRETTY_FUNCTION__,
		              ": only the first configuration is supported");
		return LIBUSB_ERROR_NOT_SUPPORTED;
	}

	Usb_device *usb_device = *(Usb_device**)device->os_priv;

	Genode::memcpy(buffer, usb_device->raw_config_descriptor, len);

	*host_endian = 0;

	return len;
}


static int genode_get_active_config_descriptor(struct libusb_device *device,
                                               unsigned char *buffer,
                                               size_t len,
                                               int *host_endian)
{
	/* only configuration 0 is currently supported */
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
	Usb_device *usb_device = *(Usb_device**)dev_handle->dev->os_priv;

	try {
		usb_device->usb_connection->claim_interface(interface_number);
	} catch (Usb::Session::Interface_not_found) {
		Genode::error(__PRETTY_FUNCTION__, ": interface not found");
		return LIBUSB_ERROR_NOT_FOUND;
	} catch (Usb::Session::Interface_already_claimed) {
		Genode::error(__PRETTY_FUNCTION__, ": interface already claimed");
		return LIBUSB_ERROR_BUSY;
	} catch (...) {
		Genode::error(__PRETTY_FUNCTION__, ": unknown exception");
		return LIBUSB_ERROR_OTHER;
	}

	return LIBUSB_SUCCESS;
}


static int genode_release_interface(struct libusb_device_handle *dev_handle,
                                    int interface_number)
{
	Usb_device *usb_device = *(Usb_device**)dev_handle->dev->os_priv;

	try {
		usb_device->usb_connection->release_interface(interface_number);
	} catch (Usb::Session::Interface_not_found) {
		Genode::error(__PRETTY_FUNCTION__, ": interface not found");
		return LIBUSB_ERROR_NOT_FOUND;
	} catch (...) {
		Genode::error(__PRETTY_FUNCTION__, ": unknown exception");
		return LIBUSB_ERROR_OTHER;
	}

	return LIBUSB_SUCCESS;
}


static int genode_set_interface_altsetting(struct libusb_device_handle* dev_handle,
                                           int interface_number,
                                           int altsetting)
{
	Usb_device *usb_device = *(Usb_device**)dev_handle->dev->os_priv;

	return usb_device->altsetting(interface_number, altsetting) ? LIBUSB_SUCCESS
	                                                            : LIBUSB_ERROR_BUSY;
}


static int genode_submit_transfer(struct usbi_transfer * itransfer)
{
	struct libusb_transfer *transfer =
		USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	Usb_device *usb_device = *(Usb_device**)transfer->dev_handle->dev->os_priv;

	if (!usb_device->usb_connection->source()->ready_to_submit())
		return LIBUSB_ERROR_BUSY;

	switch (transfer->type) {

		case LIBUSB_TRANSFER_TYPE_CONTROL: {

			struct libusb_control_setup *setup =
				(struct libusb_control_setup*)transfer->buffer;

			Usb::Packet_descriptor p;

			try {
				p = usb_device->usb_connection->alloc_packet(setup->wLength);
			} catch (Usb::Session::Tx::Source::Packet_alloc_failed) {
				return LIBUSB_ERROR_BUSY;
			}

			p.completion           = new (libc_alloc) Completion(itransfer);

			p.type                 = Usb::Packet_descriptor::CTRL;
			p.control.request      = setup->bRequest;
			p.control.request_type = setup->bmRequestType;
			p.control.value        = setup->wValue;
			p.control.index        = setup->wIndex;
			p.control.timeout      = transfer->timeout;

			if ((setup->bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) ==
			    LIBUSB_ENDPOINT_OUT) {

				char *packet_content =
					usb_device->usb_connection->source()->packet_content(p);

				Genode::memcpy(packet_content,
				               transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE,
				               setup->wLength);
			}

			usb_device->usb_connection->source()->submit_packet(p);

			return LIBUSB_SUCCESS;
		}

		case LIBUSB_TRANSFER_TYPE_BULK:
		case LIBUSB_TRANSFER_TYPE_BULK_STREAM:
		case LIBUSB_TRANSFER_TYPE_INTERRUPT: {
		
			if (IS_XFEROUT(transfer) &&
				transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET) {
				Genode::error(__PRETTY_FUNCTION__,
				              ": zero packet not supported");
				return LIBUSB_ERROR_NOT_SUPPORTED;
			}

			Usb::Packet_descriptor p;
			
			try {
				p = usb_device->usb_connection->alloc_packet(transfer->length);
			} catch (Usb::Session::Tx::Source::Packet_alloc_failed) {
				return LIBUSB_ERROR_BUSY;
			}

			if (transfer->type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
				p.type = Usb::Packet_descriptor::IRQ;
				p.transfer.polling_interval =
					Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;
			} else
				p.type = Usb::Packet_descriptor::BULK;

			p.completion  = new (libc_alloc) Completion(itransfer);
			p.transfer.ep = transfer->endpoint;

			if (IS_XFEROUT(transfer)) {
				char *packet_content =
					usb_device->usb_connection->source()->packet_content(p);
				Genode::memcpy(packet_content, transfer->buffer,
				               transfer->length);
			}

			usb_device->usb_connection->source()->submit_packet(p);

			return LIBUSB_SUCCESS;
		}

		case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: {

			size_t total_length = 0;
			for (int i = 0; i < transfer->num_iso_packets; i++) {
				total_length += transfer->iso_packet_desc[i].length;
			}

			Usb::Packet_descriptor p;
			size_t p_size = Usb::Isoc_transfer::size(total_length);
			try {
				p = usb_device->usb_connection->alloc_packet(p_size);
			} catch (Usb::Session::Tx::Source::Packet_alloc_failed) {
				return LIBUSB_ERROR_BUSY;
			}

			p.type = Usb::Packet_descriptor::ISOC;
				p.transfer.polling_interval =
					Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;

			p.completion  = new (libc_alloc) Completion(itransfer);
			p.transfer.ep = transfer->endpoint;

			Usb::Isoc_transfer &isoc =
				*(Usb::Isoc_transfer *) usb_device->usb_connection->source()->packet_content(p);
			isoc.number_of_packets = transfer->num_iso_packets;
			for (int i = 0; i < transfer->num_iso_packets; i++) {
				isoc.packet_size[i]        = transfer->iso_packet_desc[i].length;
				isoc.actual_packet_size[i] = 0;
			}

			if (IS_XFEROUT(transfer))
				Genode::memcpy(isoc.data(), transfer->buffer, transfer->length);

			usb_device->usb_connection->source()->submit_packet(p);

			return LIBUSB_SUCCESS;
		}

		default:
			usbi_err(TRANSFER_CTX(transfer),
				"unknown endpoint type %d", transfer->type);
			return LIBUSB_ERROR_INVALID_PARAM;
	}
}


static int genode_cancel_transfer(struct usbi_transfer * itransfer)
{
	return LIBUSB_SUCCESS;
}


static void genode_clear_transfer_priv(struct usbi_transfer * itransfer) { }


static int genode_handle_events(struct libusb_context *,
                            struct pollfd *,
                            POLL_NFDS_TYPE, int)
{
	if (device_instance) {
		device_instance->handle_events();
		return LIBUSB_SUCCESS;
	}

	return LIBUSB_ERROR_NO_DEVICE;
}


static int genode_handle_transfer_completion(struct usbi_transfer * itransfer)
{
	enum libusb_transfer_status status = LIBUSB_TRANSFER_COMPLETED;

	if (itransfer->flags & USBI_TRANSFER_CANCELLING)
		status = LIBUSB_TRANSFER_CANCELLED;

	return usbi_handle_transfer_completion(itransfer,
	                                       status);
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

	/*.device_priv_size =*/ sizeof(Usb_device*),
	/*.device_handle_priv_size =*/ 0,
	/*.transfer_priv_size =*/ 0,
};
