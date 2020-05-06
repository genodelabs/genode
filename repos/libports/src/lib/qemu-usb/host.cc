/**
 * \brief  USB session back end
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2015-12-18
 */

#include <base/allocator_avl.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <usb_session/connection.h>
#include <usb/usb.h>
#include <util/xml_node.h>

#include <extern_c_begin.h>
#include <qemu_emul.h>
#include <hw/usb.h>
#include <extern_c_end.h>
#include <base/debug.h>


using namespace Genode;

static bool const verbose_devices  = false;
static bool const verbose_host     = false;
static bool const verbose_warnings = false;
Lock _lock;

static void update_ep(USBDevice *);
static bool claim_interfaces(USBDevice *dev);

using Packet_alloc_failed = Usb::Session::Tx::Source::Packet_alloc_failed;
using Packet_type         = Usb::Packet_descriptor::Type;
using Packet_error        = Usb::Packet_descriptor::Error;


static unsigned endpoint_number(USBEndpoint const *usb_ep)
{
		bool in = usb_ep->pid == USB_TOKEN_IN;
		return usb_ep->nr | (in ? USB_DIR_IN : 0);

}

class Isoc_packet : Fifo<Isoc_packet>::Element
{
	friend class Fifo<Isoc_packet>;
	friend class Usb_host_device;

	private:

		Usb::Packet_descriptor _packet;
		int                    _offset { 0 };
		char                  *_content;
		int                    _size;

	public:

		Isoc_packet(Usb::Packet_descriptor packet, char *content)
		: _packet(packet), _content(content),
			_size (_packet.read_transfer() ? _packet.transfer.actual_size : _packet.size())
		{ }

		bool copy(USBPacket *usb_packet)
		{
			if (!valid()) return false;

			unsigned remaining = _size - _offset;
			int copy_size = min(usb_packet->iov.size, remaining);

			usb_packet_copy(usb_packet, _content + _offset, copy_size);

			_offset += copy_size;

			if (!_packet.read_transfer()) {
				_packet.transfer.packet_size[_packet.transfer.number_of_packets] = copy_size;
				_packet.transfer.number_of_packets++;
			}

			return remaining <= usb_packet->iov.size;
		}

		bool     valid()        const { return _content != nullptr; }
		unsigned packet_count() const { return _packet.transfer.number_of_packets; }

		Usb::Packet_descriptor& packet() { return _packet; }
};


struct Completion : Usb::Completion
{
	USBPacket *p        = nullptr;
	USBDevice *dev      = nullptr;
	uint8_t   *data     = nullptr;
	unsigned   endpoint = 0;

	enum State { VALID, FREE, CANCELED };
	State     state = FREE;

	void complete(Usb::Packet_descriptor &p) override  { }

	void complete(Usb::Packet_descriptor &packet, char *content)
	{
		if (state != VALID) {
			return;
		}

		int actual_size = 0;

		switch (packet.type) {
		case Packet_type::CTRL:
			actual_size = packet.control.actual_size;
			break;
		case Packet_type::BULK:
		case Packet_type::IRQ:
			actual_size = packet.transfer.actual_size;
		default:
			break;
		}

		if (actual_size < 0) actual_size = 0;

		if (verbose_host)
			log(__func__, ": packet: ", p, " packet.type: ", (int)packet.type, " "
			    "actual_size: ", Hex(actual_size));

		p->actual_length = 0;

		if (p->pid == USB_TOKEN_IN && actual_size > 0) {
			if (data) Genode::memcpy(data, content, actual_size);
			else      usb_packet_copy(p, content, actual_size);
		}

		p->actual_length = actual_size;

		if (packet.succeded)
			p->status = USB_RET_SUCCESS;
		else {
			if (packet.error == Packet_error::STALL_ERROR)
				p->status = USB_RET_STALL;
			else
				p->status = USB_RET_IOERROR;
		}

		switch (packet.type) {
		case Packet_type::CONFIG:
			if (!claim_interfaces(dev))
				p->status = USB_RET_IOERROR;
		case Packet_type::ALT_SETTING:
			update_ep(dev);
		case Packet_type::CTRL:
			usb_generic_async_ctrl_complete(dev, p);
			break;
		case Packet_type::BULK:
		case Packet_type::IRQ:
			usb_packet_complete(dev, p);
			break;
		default:
			break;
		}
	}

	bool     valid() const { return state == VALID; }
	void     cancel()      { state = CANCELED; }
	void     free()        { state = FREE; }
	unsigned ep() const    { return p ? endpoint_number(p->ep) : endpoint; }
};


/**
 * Helper for the formatted output and device info
 */
struct Dev_info
{
	uint32_t const vendor, product;
	uint16_t const bus, dev;

	Dev_info(uint16_t bus, uint16_t dev, uint32_t vendor, uint32_t product)
	:
		vendor(vendor), product(product), bus(bus), dev(dev)
	{ }

	void print(Output &out) const
	{
		Genode::print(out, Hex(bus, Hex::OMIT_PREFIX, Hex::PAD), ":",
		              Hex(dev, Hex::OMIT_PREFIX, Hex::PAD), " (",
		              "vendor=",  Hex(vendor, Hex::OMIT_PREFIX), ", ",
		              "product=", Hex(product, Hex::OMIT_PREFIX), ")");
	}

	bool operator != (Dev_info const &other) const
	{
		if (bus && dev)
			return bus != other.bus || dev != other.dev;

		if (vendor && product)
			return vendor != other.vendor || product != other.product;

		return true;
	}
};


struct Usb_host_device : List<Usb_host_device>::Element
{
	Usb_host_device(const Usb_host_device&);
	const Usb_host_device& operator=(const Usb_host_device&);

	struct Could_not_create_device : Exception { };

	bool           deleted = false;
	char const    *label   = nullptr;
	Dev_info const info;

	USBHostDevice  *qemu_dev { nullptr };

	/* submit queue + ack queue + 1 -> max nr of packets in flight */
	enum { NUM_COMPLETIONS = Usb::Session::TX_QUEUE_SIZE * 2 + 1 };
	Completion  completion[NUM_COMPLETIONS];

	Fifo<Isoc_packet>             isoc_read_queue { };
	Reconstructible<Isoc_packet>  isoc_write_packet { Usb::Packet_descriptor(), nullptr };

	Entrypoint  &_ep;
	Signal_handler<Usb_host_device> state_dispatcher { _ep, *this, &Usb_host_device::state_change };

	Allocator       &_alloc;
	Allocator_avl    _usb_alloc { &_alloc };
	Usb::Connection   usb_raw; //{ &_usb_alloc, label, 1024*1024, state_dispatcher };

	Signal_handler<Usb_host_device> ack_avail_dispatcher { _ep, *this, &Usb_host_device::ack_avail };

	void _release_interfaces()
	{
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		try { usb_raw.config_descriptor(&ddescr, &cdescr); }
		catch (Usb::Session::Device_not_found) { return; }

		for (unsigned i = 0; i < cdescr.num_interfaces; i++) {
			try { usb_raw.release_interface(i); }
			catch (Usb::Session::Device_not_found) { return; }
		}
	}

	bool _claim_interfaces()
	{
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		try { usb_raw.config_descriptor(&ddescr, &cdescr); }
		catch (Usb::Session::Device_not_found) { return false; }

		bool result = true;
		for (unsigned i = 0; i < cdescr.num_interfaces; i++) {
			try {
				usb_raw.claim_interface(i);
			} catch (Usb::Session::Interface_already_claimed) {
				result = false;
			}
		}

		if (!result) error("device already claimed");

		return result;
	}

	Usb_host_device(Entrypoint &ep, Allocator &alloc,
	                Env &env, char const *label,
	                Dev_info info)
	:
		label(label), _alloc(alloc),
		usb_raw(env, &_usb_alloc, label, 6*1024*1024, state_dispatcher),
		info(info), _ep(ep)
	{
		usb_raw.tx_channel()->sigh_ack_avail(ack_avail_dispatcher);

		if (!_claim_interfaces())
			throw Could_not_create_device();

		qemu_dev = create_usbdevice(this);

		if (!qemu_dev)
			throw Could_not_create_device();
	}

	~Usb_host_device()
	{
		isoc_in_flush(0, true);
	}

	static int to_qemu_speed(unsigned speed)
	{
		switch (speed) {
		case Usb::Device::SPEED_LOW:   return USB_SPEED_LOW;
		case Usb::Device::SPEED_FULL:  return USB_SPEED_FULL;
		case Usb::Device::SPEED_HIGH:  return USB_SPEED_HIGH;
		case Usb::Device::SPEED_SUPER: return USB_SPEED_SUPER;
		default: return 0;
		}
	}

	void ack_avail()
	{
		Lock::Guard g(_lock);

		/* we are already dead, do nothing */
		if (deleted == true) return;

		while (usb_raw.source()->ack_avail()) {
			Usb::Packet_descriptor packet = usb_raw.source()->get_acked_packet();
			Completion *c = dynamic_cast<Completion *>(packet.completion);

			if ((packet.type == Packet_type::ISOC && !packet.read_transfer()) ||
			    (c && c->state == Completion::CANCELED)) {
				free_packet(packet);
				continue;
			}

			char *content = usb_raw.source()->packet_content(packet);

			if (packet.type != Packet_type::ISOC) {
				c->complete(packet, content);
				free_packet(packet);
			} else {
				/* isochronous in */
				free_completion(packet);
				_isoc_in_pending--;
				Isoc_packet *new_packet = new (_alloc)
					Isoc_packet(packet, content);
				isoc_read_queue.enqueue(*new_packet);
			}
		}
	}


	/**********************************
	 ** Isochronous packet handling **
	 **********************************/

	bool isoc_read(USBPacket *packet)
	{
		if (isoc_read_queue.empty()) {
			return false;
		}

		isoc_read_queue.head([&] (Isoc_packet &head) {
			if (head.copy(packet)) {
				isoc_read_queue.remove(head);
				free_packet(&head);
			}
		});

		return true;
	}

	unsigned _isoc_in_pending = 0;

	bool isoc_new_packet()
	{
		unsigned count = 0;
		isoc_read_queue.for_each([&count] (Isoc_packet&) { count++; });
		return (count + _isoc_in_pending) < 3 ? true : false;
	}

	void isoc_in_packet(USBPacket *usb_packet)
	{
		enum { NUMBER_OF_PACKETS = 2 };
		isoc_read(usb_packet);

		if (!isoc_new_packet())
			return;

		size_t size = usb_packet->ep->max_packet_size * NUMBER_OF_PACKETS;
		try {
			Usb::Packet_descriptor packet     = alloc_packet(size);
			packet.type                       = Packet_type::ISOC;
			packet.transfer.ep                = usb_packet->ep->nr | USB_DIR_IN;
			packet.transfer.polling_interval  = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;
			packet.transfer.number_of_packets = NUMBER_OF_PACKETS;
			for (unsigned i = 0; i < NUMBER_OF_PACKETS; i++) {
				packet.transfer.packet_size[i] = usb_packet->ep->max_packet_size;
			}

			Completion *c = dynamic_cast<Completion *>(packet.completion);
			c->p          = nullptr;
			c->dev        = usb_packet->ep->dev;
			c->data       = nullptr;
			c->endpoint   = endpoint_number(usb_packet->ep);

			_isoc_in_pending++;

			submit(packet);
		} catch (Packet_alloc_failed) {
			if (verbose_warnings)
				warning("xHCI: packet allocation failed (size ", Hex(size), "in ", __func__, ")");
		}
	}

	void isoc_in_flush(unsigned ep, bool all = false)
	{
		/* flush finished and stored data */
		isoc_read_queue.for_each([&] (Isoc_packet &packet) {
			if (!all && (!packet.valid() || packet.packet().transfer.ep != ep)) {
				return;
			}

			isoc_read_queue.remove(packet);
			free_packet(&packet);
		});

		/* flush in flight packets */
		flush_completion(ep);
	}

	void isoc_out_packet(USBPacket *usb_packet)
	{
		enum { NUMBER_OF_PACKETS = 32 };

		bool valid = isoc_write_packet->valid();
		if (valid) {
			isoc_write_packet->copy(usb_packet);

			if (isoc_write_packet->packet_count() < NUMBER_OF_PACKETS)
				return;

			submit(isoc_write_packet->packet());
		}

		size_t size = usb_packet->ep->max_packet_size * NUMBER_OF_PACKETS;
		try {
			Usb::Packet_descriptor packet     = alloc_packet(size, false);
			packet.type                       = Packet_type::ISOC;
			packet.transfer.ep                = usb_packet->ep->nr;
			packet.transfer.polling_interval  = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;
			packet.transfer.number_of_packets = 0;

			isoc_write_packet.construct(packet, usb_raw.source()->packet_content(packet));
			if (!valid) isoc_write_packet->copy(usb_packet);

		} catch (Packet_alloc_failed) {
			if (verbose_warnings)
				warning("xHCI: packet allocation failed (size ", Hex(size), "in ", __func__, ")");
			isoc_write_packet.construct(Usb::Packet_descriptor(), nullptr);
			return;
		}
	}


	/***********************
	 ** Device disconnect **
	 ***********************/

	void _destroy()
	{
		/* mark delete before removing */
		deleted = true;

		/* remove from USB bus */
		remove_usbdevice(qemu_dev);
		qemu_dev = nullptr;
	}

	void state_change()
	{
		Lock::Guard g(_lock);
		if (usb_raw.plugged())
			return;

		_destroy();
	}

	void destroy()
	{
		Lock::Guard g(_lock);

		_release_interfaces();

		_destroy();
	}


	/*******************
	 ** Packet stream **
	 *******************/

	Usb::Packet_descriptor alloc_packet(int length, bool completion = true)
	{
		if (!usb_raw.source()->ready_to_submit()) {
			throw Packet_alloc_failed();
		}

		Usb::Packet_descriptor packet = usb_raw.source()->alloc_packet(length);

		if (!completion) {
			packet.completion = nullptr;
			return packet;
		}

		packet.completion = alloc_completion();
		if (!packet.completion) {
			usb_raw.source()->release_packet(packet);
			throw Packet_alloc_failed();
		}

		return packet;
	}

	void free_packet(Usb::Packet_descriptor &packet)
	{
		free_completion(packet);
		usb_raw.source()->release_packet(packet);
	}

	void free_packet(Isoc_packet *packet)
	{
		free_packet(packet->packet());
		Genode::destroy(_alloc, packet);
	}

	Completion *alloc_completion()
	{
		for (unsigned i = 0; i < NUM_COMPLETIONS; i++)
			if (completion[i].state == Completion::FREE) {
				completion[i].state = Completion::VALID;
				return &completion[i];
			}

		return nullptr;
	}

	void free_completion(Usb::Packet_descriptor &packet)
	{
		if (packet.completion) {
			dynamic_cast<Completion *>(packet.completion)->free();
		}
	}

	Completion *find_valid_completion(USBPacket *p)
	{
		for (unsigned i = 0; i < NUM_COMPLETIONS; i++)
			if (completion[i].p == p && completion[i].valid())
				return &completion[i];

		return nullptr;
	}

	void flush_completion(unsigned ep)
	{
		for (unsigned i = 0; i < NUM_COMPLETIONS; i++)
		{
			if (!completion[i].valid())
				continue;

			if (completion[i].ep() == ep) {
				completion[i].cancel();
			}
		}
	}

	void submit(Usb::Packet_descriptor p)
	{
		usb_raw.source()->submit_packet(p);
	}

	bool claim_interfaces() { return _claim_interfaces(); }

	void set_configuration(uint8_t value, USBPacket *p)
	{
		_release_interfaces();

		try {
			Usb::Packet_descriptor packet = alloc_packet(0);
			packet.type   =  Usb::Packet_descriptor::CONFIG;
			packet.number = value;

			Completion *c = dynamic_cast<Completion *>(packet.completion);
			c->p = p;
			c->dev = cast_USBDevice(qemu_dev);
			submit(packet);
			p->status = USB_RET_ASYNC;
		} catch (...) {
			if (verbose_warnings)
				warning(__func__, " packet allocation failed");
			p->status = USB_RET_NAK;
		}

	}

	void set_interface(int index, uint8_t value, USBPacket *p)
	{
		try {
			Usb::Packet_descriptor packet = alloc_packet(0);
			packet.type                  = Usb::Packet_descriptor::ALT_SETTING;
			packet.interface.number      = index;
			packet.interface.alt_setting = value;

			Completion *c = dynamic_cast<Completion *>(packet.completion);
			c->p = p;
			c->dev = cast_USBDevice(qemu_dev);
			submit(packet);
			p->status = USB_RET_ASYNC;
		} catch (...) {
			if (verbose_warnings)
				warning(__func__, " packet allocation failed");
			p->status = USB_RET_NAK;
		}
	}

	void update_ep(USBDevice *udev)
	{
		usb_ep_reset(udev);

		/* retrieve device speed */
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		try { usb_raw.config_descriptor(&ddescr, &cdescr); }
		catch (Usb::Session::Device_not_found) { return; }

		for (unsigned i = 0; i < cdescr.num_interfaces; i++) {
			udev->altsetting[i] = usb_raw.alt_settings(i);
		}

		for (unsigned i = 0; i < cdescr.num_interfaces; i++) {
			for (int j = 0; j < udev->altsetting[i]; j++) {
				Usb::Interface_descriptor iface;
				usb_raw.interface_descriptor(i, j, &iface);
				for (unsigned k = 0; k < iface.num_endpoints; k++) {
					Usb::Endpoint_descriptor endp;
					usb_raw.endpoint_descriptor(i, j, k, &endp);

					int const pid      = (endp.address & USB_DIR_IN) ? USB_TOKEN_IN : USB_TOKEN_OUT;
					int const ep       = (endp.address & 0xf);
					uint8_t const type = (endp.attributes & 0x3);

					usb_ep_set_max_packet_size(udev, pid, ep, endp.max_packet_size);
					usb_ep_set_type(udev, pid, ep, type);
					usb_ep_set_ifnum(udev, pid, ep, i);
					usb_ep_set_halted(udev, pid, ep, 0);
				}
			}
		}
	}
};


/********************
 ** Qemu interface **
 ********************/

#define TRACE_AND_STOP do { warning(__func__, " not implemented"); } while(false)

#define USB_HOST_DEVICE(obj) \
        OBJECT_CHECK(USBHostDevice, (obj), TYPE_USB_HOST_DEVICE)


static void update_ep(USBDevice *udev)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;

	dev->update_ep(udev);
}


static bool claim_interfaces(USBDevice *udev)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;

	return dev->claim_interfaces();
}


static void usb_host_realize(USBDevice *udev, Error **errp)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;

	/* retrieve device speed */
	Usb::Config_descriptor cdescr;
	Usb::Device_descriptor ddescr;

	try { dev->usb_raw.config_descriptor(&ddescr, &cdescr); }
	catch (Usb::Session::Device_not_found) { return; }

	if (verbose_host)
		log("set udev->speed to %d", Usb_host_device::to_qemu_speed(ddescr.speed));

	udev->speed     = Usb_host_device::to_qemu_speed(ddescr.speed);
	udev->speedmask = (1 << udev->speed);

	udev->flags |= (1 << USB_DEV_FLAG_IS_HOST);

	dev->update_ep(udev);
}


static void usb_host_cancel_packet(USBDevice *udev, USBPacket *p)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;
	Completion        *c = dev->find_valid_completion(p);

	if (c)
		c->cancel();
}


static void usb_host_handle_data(USBDevice *udev, USBPacket *p)
{
	USBHostDevice               *d    = USB_HOST_DEVICE(udev);
	Usb_host_device             *dev  = (Usb_host_device *)d->data;
	Genode::size_t               size = 0;
	Usb::Packet_descriptor::Type type = Usb::Packet_descriptor::BULK;
	bool const                   in   = p->pid == USB_TOKEN_IN;

	switch (usb_ep_get_type(udev, p->pid, p->ep->nr)) {
	case USB_ENDPOINT_XFER_BULK:
		type      = Usb::Packet_descriptor::BULK;
		size      = usb_packet_size(p);
		p->status = USB_RET_ASYNC;
		break;
	case USB_ENDPOINT_XFER_INT:
		type      = Usb::Packet_descriptor::IRQ;
		size      = p->iov.size;
		p->status = USB_RET_ASYNC;
		break;
	case USB_ENDPOINT_XFER_ISOC:
		if (in)
			dev->isoc_in_packet(p);
		else
			dev->isoc_out_packet(p);
		return;
	default:
		error("not supported data request");
		break;
	}

	try {
		Usb::Packet_descriptor packet     = dev->alloc_packet(size);
		packet.type                       = type;
		packet.transfer.ep                = p->ep->nr | (in ? USB_DIR_IN : 0);
		packet.transfer.polling_interval  = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;

		if (!in) {
			char * const content = dev->usb_raw.source()->packet_content(packet);
			usb_packet_copy(p, content, size);
		}

		Completion *c = dynamic_cast<Completion *>(packet.completion);
		c->p          = p;
		c->dev        = udev;
		c->data       = nullptr;

		dev->submit(packet);
	} catch (Packet_alloc_failed) {
		if (verbose_warnings)
			warning("xHCI: packet allocation failed (size ", Hex(size), "in ", __func__, ")");
		p->status = USB_RET_NAK;
	}
}


static void usb_host_handle_control(USBDevice *udev, USBPacket *p,
                                    int request, int value, int index,
                                    int length, uint8_t *data)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;

	if (verbose_host)
		log("r: ", Hex(request), " v: ", Hex(value), " "
		    "i: ", Hex(index), " length: ", length);

	switch (request) {
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		udev->addr = value;
		return;
	case DeviceOutRequest | USB_REQ_SET_CONFIGURATION:
		dev->set_configuration(value & 0xff, p);
		return;
	case InterfaceOutRequest | USB_REQ_SET_INTERFACE:
		dev->set_interface(index, value, p);
		return;
	}

	if (udev->speed == USB_SPEED_SUPER &&
		!(udev->port->speedmask & USB_SPEED_MASK_SUPER) &&
		request == 0x8006 && value == 0x100 && index == 0) {
		error("r->usb3ep0quirk = true");
	}

	Usb::Packet_descriptor packet;
	try {
		packet = dev->alloc_packet(length);
	} catch (...) {
		if (verbose_warnings)
			warning("Packet allocation failed");
		return;
	}

	packet.type = Usb::Packet_descriptor::CTRL;
	packet.control.request_type = request >> 8;
	packet.control.request      = request & 0xff;
	packet.control.index        = index;
	packet.control.value        = value;

	Completion *c = dynamic_cast<Completion *>(packet.completion);
	c->p        = p;
	c->dev      = udev;
	c->data     = data;

	if (!(packet.control.request_type & USB_DIR_IN) && length) {
		char *packet_content = dev->usb_raw.source()->packet_content(packet);
		Genode::memcpy(packet_content, data, length);
	}

	dev->submit(packet);
	p->status = USB_RET_ASYNC;
}


static void usb_host_ep_stopped(USBDevice *udev, USBEndpoint *usb_ep)
{
	USBHostDevice               *d    = USB_HOST_DEVICE(udev);
	Usb_host_device             *dev  = (Usb_host_device *)d->data;

	bool     in = usb_ep->pid == USB_TOKEN_IN;
	unsigned ep = endpoint_number(usb_ep);

	switch (usb_ep->type) {
	case USB_ENDPOINT_XFER_ISOC:
		if (in)
			dev->isoc_in_flush(ep);
	default:
		return;
	}
}


static Property usb_host_dev_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};


static void usb_host_class_initfn(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);
	USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

	uc->realize        = usb_host_realize;
	uc->product_desc   = "USB Host Device";
	uc->cancel_packet  = usb_host_cancel_packet;
	uc->handle_data    = usb_host_handle_data;
	uc->handle_control = usb_host_handle_control;
	uc->ep_stopped     = usb_host_ep_stopped;
	dc->props = usb_host_dev_properties;
}


static TypeInfo usb_host_dev_info;


static void usb_host_register_types(void)
{
	usb_host_dev_info.name          = TYPE_USB_HOST_DEVICE;
	usb_host_dev_info.parent        = TYPE_USB_DEVICE;
	usb_host_dev_info.instance_size = sizeof(USBHostDevice);
	usb_host_dev_info.class_init    = usb_host_class_initfn;

	type_register_static(&usb_host_dev_info);
}


struct Usb_devices : List<Usb_host_device>
{
	Entrypoint                    &_ep;
	Env                           &_env;
	Allocator                     &_alloc;
	Signal_handler<Usb_devices>    _device_dispatcher { _ep, *this, &Usb_devices::_devices_update };
	Attached_rom_dataspace         _devices_rom = { _env, "usb_devices" };

	void _garbage_collect()
	{
		for (Usb_host_device *next = first(); next;) {
			Usb_host_device *current = next;
			next = current->next();

			if (current->deleted == false)
				continue;

			remove(current);
			Genode::destroy(_alloc, current);
		}
	}

	template <typename FUNC>
	void for_each(FUNC  const &fn)
	{
		for (Usb_host_device *d = first(); d; d = d->next())
			fn(*d);
	}

	void _devices_update()
	{
		Lock::Guard g(_lock);

		_garbage_collect();

		_devices_rom.update();
		if (!_devices_rom.valid())
			return;

		for_each([] (Usb_host_device &device) {
			device.deleted = true;
		});

		if (verbose_devices)
			log(_devices_rom.local_addr<char const>());

		Xml_node devices_node(_devices_rom.local_addr<char>(), _devices_rom.size());
		devices_node.for_each_sub_node("device", [&] (Xml_node const &node) {

			unsigned product = node.attribute_value<unsigned>("product_id", 0);
			unsigned vendor  = node.attribute_value<unsigned>("vendor_id", 0);
			unsigned bus     = node.attribute_value<unsigned>("bus", 0);
			unsigned dev     = node.attribute_value<unsigned>("dev", 0);

			Dev_info const dev_info(bus, dev, vendor, product);

			if (!node.has_attribute("label")) {
				error("no label found for device ", dev_info);
				return;
			}

			typedef String<128> Label;
			Label const label = node.attribute_value("label", Label());

			/* ignore if already created */
			bool exists = false;
			for_each([&] (Usb_host_device &device) {
				if (device.info != dev_info)
					return;

				exists         = true;
				device.deleted = false;
			});

			if (exists)
				return;

			try {
				Usb_host_device *new_device = new (_alloc)
					Usb_host_device(_ep, _alloc, _env, label.string(),
					                dev_info);

				insert(new_device);

				log("Attach USB device ", dev_info);
			} catch (...) {
				error("could not attach USB device ", dev_info);
			}
		});

		/* remove devices deleted by update */
		for_each([] (Usb_host_device &device) {
			if (device.deleted == false) return;

			device._release_interfaces();
			device._destroy();
		});
		_garbage_collect();
	}

	Usb_devices(Entrypoint &ep, Allocator &alloc, Env &env)
	: _ep(ep), _env(env), _alloc(alloc)
	{
		_devices_rom.sigh(_device_dispatcher);
	}

	void destroy()
	{
		for (Usb_host_device *d = first(); d; d = d->next())
			d->destroy();

		_garbage_collect();
	}
};


static Usb_devices *_devices;


extern "C" void usb_host_destroy()
{
	if (_devices == nullptr) return;

	_devices->destroy();
}


extern "C" void usb_host_update_devices()
{
	if (_devices == nullptr) return;

	_devices->_devices_update();
}


/*
 * Do not use type_init macro because of name mangling
 */
extern "C" void _type_init_usb_host_register_types(Entrypoint *ep,
                                                   Allocator *alloc,
                                                   Env *env)
{
	usb_host_register_types();

	static Usb_devices devices(*ep, *alloc, *env);
	_devices = &devices;
}
