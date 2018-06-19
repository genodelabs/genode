/*
 * \brief  USB URB implementation
 * \author Stefan Kalkowski
 * \date   2018-06-13
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__USB_H_
#define _LX_KIT__USB_H_

/* Genode includes */
#include <usb_session/client.h>
#include <util/string.h>


class Urb : public Usb::Completion
{
	protected:

		Usb::Session_client  & _usb;
		urb                  & _urb;
		Usb::Packet_descriptor _packet {
			_usb.source()->alloc_packet(_urb.transfer_buffer_length) };

	public:

		Urb(Usb::Session_client &usb, urb & urb) : _usb(usb), _urb(urb)
		{
			_packet.completion = this;

			switch(usb_pipetype(_urb.pipe)) {
			case PIPE_INTERRUPT:
				{
					struct usb_host_endpoint *ep =
						usb_pipe_endpoint(_urb.dev, _urb.pipe);
					_packet.type = Usb::Packet_descriptor::IRQ;
					_packet.transfer.polling_interval = _urb.interval;
					_packet.transfer.ep = ep->desc.bEndpointAddress;
					return;
				}

			case PIPE_CONTROL:
				{
					usb_ctrlrequest * ctrl = (usb_ctrlrequest *)
						_urb.setup_packet;
					_packet.type = Usb::Packet_descriptor::CTRL;
					_packet.control.request      = ctrl->bRequest;
					_packet.control.request_type = ctrl->bRequestType;
					_packet.control.value        = ctrl->wValue;
					_packet.control.index        = ctrl->wIndex;

					if (!(ctrl->bRequestType & USB_DIR_IN))
						Genode::memcpy(_usb.source()->packet_content(_packet),
									   _urb.transfer_buffer, _urb.transfer_buffer_length);
					return;
				}

			case PIPE_BULK:
				{
					struct usb_host_endpoint *ep =
						usb_pipe_endpoint(_urb.dev, _urb.pipe);
					_packet.type = Usb::Packet_descriptor::BULK;
					_packet.transfer.ep = ep->desc.bEndpointAddress;

					if (usb_pipeout(_urb.pipe))
						Genode::memcpy(_usb.source()->packet_content(_packet),
									   _urb.transfer_buffer, _urb.transfer_buffer_length);
					return;
				}

			default:
				Genode::error("unknown URB requested");
			};
		}

		void send()
		{
			_usb.source()->submit_packet(_packet);
		}

		void complete(Usb::Packet_descriptor & packet) override
		{
			if (packet.succeded) {
				bool ctrl = (usb_pipetype(_urb.pipe) == PIPE_CONTROL);
				_urb.actual_length = ctrl ? packet.control.actual_size
				                          : packet.transfer.actual_size;

				if (_urb.actual_length && _urb.transfer_buffer &&
				    (_urb.transfer_buffer_length >= _urb.actual_length))
					Genode::memcpy(_urb.transfer_buffer,
					               _usb.source()->packet_content(packet),
					               _urb.actual_length);
			}

			if (_urb.complete) _urb.complete(&_urb);
			kfree(_packet.completion);
		}
};


class Sync_ctrl_urb : public Urb
{
	private:

		completion _comp;

	public:

		Sync_ctrl_urb(Usb::Session_client & usb, urb & urb) : Urb(usb, urb) {
			init_completion(&_comp); }

		void complete(Usb::Packet_descriptor &p) override
		{
			Urb::complete(p);
			::complete(&_comp);
		}

		void send(int timeout)
		{
			_packet.completion      = this;
			_packet.control.timeout = timeout;
			Urb::send();
			wait_for_completion(&_comp);
		}
};

#endif /* _LX_KIT__USB_H_ */
