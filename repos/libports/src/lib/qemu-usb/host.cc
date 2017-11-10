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


using namespace Genode;

static bool const verbose_devices = false;
static bool const verbose_host    = false;
Lock _lock;


static void update_ep(USBDevice *);
static bool claim_interfaces(USBDevice *dev);


struct Completion : Usb::Completion
{
	USBPacket *p    = nullptr;
	USBDevice *dev  = nullptr;
	uint8_t   *data = nullptr;
	enum State { VALID, FREE, CANCELED };
	State     state = FREE;

	void complete(Usb::Packet_descriptor &p) override  { }

	void complete(Usb::Packet_descriptor &packet, char *content)
	{
		if (state != VALID)
			return;

		int actual_size = 0;

		switch (packet.type) {
		case Usb::Packet_descriptor::CTRL:
			actual_size = packet.control.actual_size;
			break;
		case Usb::Packet_descriptor::BULK:
		case Usb::Packet_descriptor::IRQ:
			actual_size = packet.transfer.actual_size;
		default:
			break;
		}

		if (actual_size < 0) actual_size = 0;

		if (verbose_host)
			log(__func__, ": packet.type: ", (int)packet.type, " "
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
			if (packet.error == Usb::Packet_descriptor::STALL_ERROR)
				p->status = USB_RET_STALL;
			else
				p->status = USB_RET_IOERROR;
		}

		switch (packet.type) {
		case Usb::Packet_descriptor::CONFIG:
			if (!claim_interfaces(dev))
				p->status = USB_RET_IOERROR;
		case Usb::Packet_descriptor::ALT_SETTING:
			update_ep(dev);
		case Usb::Packet_descriptor::CTRL:
			usb_generic_async_ctrl_complete(dev, p);
			break;
		case Usb::Packet_descriptor::BULK:
		case Usb::Packet_descriptor::IRQ:
			usb_packet_complete(dev, p);
			break;
		default:
			break;
		}
	}
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

	void print(Genode::Output &out) const
	{
		Genode::print(out, Hex(bus, Hex::OMIT_PREFIX, Hex::PAD), ":",
			               Hex(dev, Hex::OMIT_PREFIX, Hex::PAD), " (",
			               "vendor=",  Hex(vendor, Hex::OMIT_PREFIX), ", ",
			               "product=", Hex(product, Hex::OMIT_PREFIX), ")");
	}

	bool operator != (Dev_info const &other) const
	{
		if (bus && dev)
			return bus != other.bus && dev != other.dev;

		if (vendor && product)
			return vendor != other.vendor && product != other.product;

		return true;
	}
};


struct Usb_host_device : List<Usb_host_device>::Element
{
	struct Could_not_create_device : Genode::Exception { };

	bool           deleted = false;
	char const    *label   = nullptr;
	Dev_info const info;


	USBHostDevice  *qemu_dev;
	Completion      completion[Usb::Session::TX_QUEUE_SIZE];

	Signal_receiver  &sig_rec;
	Signal_dispatcher<Usb_host_device> state_dispatcher { sig_rec, *this, &Usb_host_device::state_change };

	Allocator_avl   _alloc;
	Usb::Connection usb_raw; // { &alloc, label, 1024*1024, state_dispatcher };

	Signal_dispatcher<Usb_host_device> ack_avail_dispatcher { sig_rec, *this, &Usb_host_device::ack_avail };

	void _release_interfaces()
	{
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		usb_raw.config_descriptor(&ddescr, &cdescr);

		for (unsigned i = 0; i < cdescr.num_interfaces; i++) {
			usb_raw.release_interface(i);
		}
	}

	bool _claim_interfaces()
	{
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		usb_raw.config_descriptor(&ddescr, &cdescr);

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

	Usb_host_device(Signal_receiver &sig_rec, Allocator &alloc,
	                Genode::Env &env, char const *label,
	                Dev_info info)
	:
		label(label), _alloc(&alloc),
		usb_raw(env, &_alloc, label, 6*1024*1024, state_dispatcher),
		info(info), sig_rec(sig_rec)
	{
		usb_raw.tx_channel()->sigh_ack_avail(ack_avail_dispatcher);

		if (!_claim_interfaces())
			throw Could_not_create_device();

		qemu_dev = create_usbdevice(this);

		if (!qemu_dev)
			throw Could_not_create_device();
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

	void ack_avail(unsigned)
	{
		Lock::Guard g(_lock);

		/* we are already dead, do nothing */
		if (deleted == true) return;

		while (usb_raw.source()->ack_avail()) {
			Usb::Packet_descriptor packet = usb_raw.source()->get_acked_packet();

			char *packet_content = usb_raw.source()->packet_content(packet);
			dynamic_cast<Completion *>(packet.completion)->complete(packet, packet_content);
			free_packet(packet);
		}
	}

	void _destroy()
	{
		/* mark delete before removing */
		deleted = true;

		/* remove from USB bus */
		remove_usbdevice(qemu_dev);
		qemu_dev = nullptr;
	}

	void state_change(unsigned)
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

	Usb::Packet_descriptor alloc_packet(int length)
	{

		if (!usb_raw.source()->ready_to_submit())
			throw -1;

		Usb::Packet_descriptor packet = usb_raw.source()->alloc_packet(length);
		packet.completion = alloc_completion();
		return packet;
	}

	void free_packet(Usb::Packet_descriptor &packet)
	{
		dynamic_cast<Completion *>(packet.completion)->state = Completion::FREE;
		usb_raw.source()->release_packet(packet);
	}

	Completion *alloc_completion()
	{
		for (unsigned i = 0; i < Usb::Session::TX_QUEUE_SIZE; i++)
			if (completion[i].state == Completion::FREE) {
				completion[i]. state = Completion::VALID;
				return &completion[i];
			}

		return nullptr;
	}

	Completion *find_completion(USBPacket *p)
	{
		for (unsigned i = 0; i < Usb::Session::TX_QUEUE_SIZE; i++)
			if (completion[i].p == p)
				return &completion[i];

		return nullptr;
	}

	void submit(Usb::Packet_descriptor p) {
		usb_raw.source()->submit_packet(p); }

	bool claim_interfaces() { return _claim_interfaces(); }

	void set_configuration(uint8_t value, USBPacket *p)
	{
		_release_interfaces();

		Usb::Packet_descriptor packet = alloc_packet(0);
		packet.type   =  Usb::Packet_descriptor::CONFIG;
		packet.number = value;

		Completion *c = dynamic_cast<Completion *>(packet.completion);
		c->p = p;
		c->dev = cast_USBDevice(qemu_dev);
		submit(packet);
		p->status = USB_RET_ASYNC;
	}

	void set_interface(int index, uint8_t value, USBPacket *p)
	{
		Usb::Packet_descriptor packet = alloc_packet(0);
		packet.type                  = Usb::Packet_descriptor::ALT_SETTING;
		packet.interface.number      = index;
		packet.interface.alt_setting = value;

		Completion *c = dynamic_cast<Completion *>(packet.completion);
		c->p = p;
		c->dev = cast_USBDevice(qemu_dev);
		submit(packet);
		p->status = USB_RET_ASYNC;
	}

	void update_ep(USBDevice *udev)
	{
		usb_ep_reset(udev);

		/* retrieve device speed */
		Usb::Config_descriptor cdescr;
		Usb::Device_descriptor ddescr;

		usb_raw.config_descriptor(&ddescr, &cdescr);

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

	dev->usb_raw.config_descriptor(&ddescr, &cdescr);

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
	Completion        *c = dev->find_completion(p);
	c->state = Completion::CANCELED;
}


static void usb_host_handle_data(USBDevice *udev, USBPacket *p)
{
	USBHostDevice     *d = USB_HOST_DEVICE(udev);
	Usb_host_device *dev = (Usb_host_device *)d->data;

	Genode::size_t size = 0;
	Usb::Packet_descriptor::Type type = Usb::Packet_descriptor::BULK;

	switch (usb_ep_get_type(udev, p->pid, p->ep->nr)) {
	case USB_ENDPOINT_XFER_BULK:
		type = Usb::Packet_descriptor::BULK;
		size = usb_packet_size(p);
		break;
	case USB_ENDPOINT_XFER_INT:
		type = Usb::Packet_descriptor::IRQ;
		size = p->iov.size;
		break;
	default:
		error("not supported data request");
		break;
	}

	bool const in = p->pid == USB_TOKEN_IN;

	try {
		Usb::Packet_descriptor packet = dev->alloc_packet(size);
		packet.type                      = type;
		packet.transfer.ep               = p->ep->nr | (in ? USB_DIR_IN : 0);
		packet.transfer.polling_interval = Usb::Packet_descriptor::DEFAULT_POLLING_INTERVAL;

		if (!in) {
			char * const content = dev->usb_raw.source()->packet_content(packet);
			usb_packet_copy(p, content, size);
		}

		Completion *c = dynamic_cast<Completion *>(packet.completion);
		c->p          = p;
		c->dev        = udev;
		c->data       = nullptr;

		dev->submit(packet);
		p->status = USB_RET_ASYNC;
	} catch (...) {
		/* submit queue full or packet larger than packet stream */
		Genode::warning("xHCI: packet allocation failed (size ", Genode::Hex(size), ")");
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
	} catch (...) { error("Packet allocation failed"); return; }

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
	Signal_receiver               &_sig_rec;
	Env                           &_env;
	Allocator                     &_alloc;
	Signal_dispatcher<Usb_devices> _device_dispatcher { _sig_rec, *this, &Usb_devices::_devices_update };
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

	void _devices_update(unsigned)
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

			Genode::String<128> label;
			try {
				node.attribute("label").value(&label);
			} catch (Genode::Xml_attribute::Nonexistent_attribute) {
				error("no label found for device ", dev_info);
				return;
			}

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
					Usb_host_device(_sig_rec, _alloc, _env, label.string(),
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

	Usb_devices(Signal_receiver *sig_rec, Allocator &alloc, Genode::Env &env)
	: _sig_rec(*sig_rec), _env(env), _alloc(alloc)
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

	_devices->_devices_update(0);
}


/*
 * Do not use type_init macro because of name mangling
 */
extern "C" void _type_init_usb_host_register_types(Genode::Signal_receiver *sig_rec,
                                                   Genode::Allocator *alloc,
                                                   Genode::Env *env)
{
	usb_host_register_types();

	static Usb_devices devices(sig_rec, *alloc, *env);
	_devices = &devices;
}
