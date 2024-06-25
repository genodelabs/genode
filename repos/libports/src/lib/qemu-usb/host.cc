/**
 * \brief  USB session back end
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2015-12-18
 */

#include <base/allocator_avl.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <usb_session/device.h>
#include <util/list_model.h>
#include <util/xml_node.h>

#include <extern_c_begin.h>
#include <qemu_emul.h>
#include <hw/usb.h>
#include <desc.h>
#include <extern_c_end.h>

using namespace Genode;

Mutex _mutex;

using handle_t = unsigned long;

class Device;
class Interface;
class Endpoint;


class Urb : Usb::Endpoint, public Usb::Interface::Urb,
            Registry<Urb>::Element
{
	private:

		friend class ::Interface;
		using Pdesc = Usb::Interface::Packet_descriptor;

		static Pdesc::Type _type(uint8_t t)
		{
			switch(t) {
				case USB_ENDPOINT_XFER_BULK:  return Pdesc::BULK;
				case USB_ENDPOINT_XFER_INT:   return Pdesc::IRQ;
				case USB_ENDPOINT_XFER_ISOC:  return Pdesc::ISOC;
				default:                      return Pdesc::FLUSH;
			};
			return Pdesc::FLUSH;
		}

		::Endpoint       &_endpoint;
		USBPacket * const _packet   { nullptr };
		bool              _canceled { false   };

	public:

		Urb(Registry<Urb> &registry,
		    ::Interface   &iface,
		    ::Endpoint    &endp,
		    uint8_t        type,
		    size_t         size,
		    USBPacket     *packet);

		Urb(Registry<Urb> &registry,
		    ::Interface   &iface,
		    ::Endpoint    &endp,
		    uint8_t        type,
		    size_t         size,
		    uint32_t       isoc_packets);

		bool isoc() const { return Usb::Interface::Urb::_type == Pdesc::ISOC; }
		void cancel() { _canceled = true; }
		bool canceled() const { return _canceled; }
		USBPacket *packet() const { return _packet; }

		size_t read_cache(Byte_range_ptr &dst);
		void   write_cache(Const_byte_range_ptr const &src);
		void   destroy();
};


class Isoc_cache
{
	public:

		enum {
			PACKETS_PER_URB = 32,
			URBS            = 4,
			MAX_PACKETS     = 256,
		};

	private:

		::Interface   &_iface;
		Endpoint      &_ep;
		Allocator     &_alloc;
		uint8_t        _read  { 0 };
		uint8_t        _wrote { 0 };
		uint16_t       _sizes[MAX_PACKETS];
		unsigned char *_buffer;

		Constructible<Urb> _urbs[URBS];

		void _new_urb();

		uint8_t _level() const;

		void _copy_to_host(USBPacket *p);
		void _copy_to_guest(USBPacket *p);

	public:

		Isoc_cache(::Interface &iface, Endpoint &ep, Allocator &alloc);

		void   handle(USBPacket *p);
		size_t read(Byte_range_ptr &dst);
		void   write(Const_byte_range_ptr const &src);
		void   destroy(Urb * urb);
		void   flush();
};


class Endpoint : public List_model<Endpoint>::Element
{
	private:

		uint8_t const             _address;
		uint8_t const             _attributes;
		uint8_t const             _max_packet_size;
		Constructible<Isoc_cache> _isoc_cache { };

	public:

		Endpoint(Xml_node const &n, Allocator &alloc, ::Interface &iface)
		:
			_address(n.attribute_value<uint8_t>("address", 0xff)),
			_attributes(n.attribute_value<uint8_t>("attributes", 0xff)),
			_max_packet_size(n.attribute_value<uint8_t>("max_packet_size", 0))
		{
			if ((_attributes&0x3) == Usb::Endpoint::ISOC)
				_isoc_cache.construct(iface, *this, alloc);
		}

		uint8_t address()         const { return _address; }
		uint8_t attributes()      const { return _attributes; }
		uint8_t max_packet_size() const { return _max_packet_size; }

		bool matches(Xml_node const &node) const {
			return address() == node.attribute_value<uint8_t>("address", 0xff); }

		static bool type_matches(Xml_node const &node) {
			return node.has_type("endpoint"); }

		bool in() const { return address() & (1<<7); }

		void handle_isoc_packet(USBPacket *p) {
			if (_isoc_cache.constructed()) _isoc_cache->handle(p); }

		size_t read_cache(Byte_range_ptr &dst) {
			return (_isoc_cache.constructed()) ? _isoc_cache->read(dst) : 0; }

		void write_cache(Const_byte_range_ptr const &src) {
			if (_isoc_cache.constructed()) _isoc_cache->write(src); }

		void destroy_urb(Urb * urb) {
			if (_isoc_cache.constructed()) _isoc_cache->destroy(urb); }

		void flush() {
			if (_isoc_cache.constructed()) _isoc_cache->flush(); }
};


class Interface : public List_model<::Interface>::Element
{
	private:

		friend class Urb;

		Device                       &_device;
		Constructible<Usb::Interface> _iface {};
		List_model<Endpoint>          _endpoints {};
		uint8_t const                 _number;
		uint8_t const                 _alt_setting;
		bool                          _active;
		size_t                        _buf_size { 2 * 1024 * 1024 };

		Usb::Interface &_session();

	public:

		Interface(Device &device, Xml_node const &n)
		:
			_device(device),
			_number(n.attribute_value<uint8_t>("number", 0xff)),
			_alt_setting(n.attribute_value<uint8_t>("alt_setting", 0xff)),
			_active(n.attribute_value("active", false)) {}

		void destroy_all_urbs();

		uint8_t number()      const { return _number; };
		uint8_t alt_setting() const { return _alt_setting; };
		bool    active()      const { return _active; }

		bool matches(Xml_node const &n) const
		{
			uint8_t nr  = n.attribute_value<uint8_t>("number", 0xff);
			uint8_t alt = n.attribute_value<uint8_t>("alt_setting", 0xff);
			return _number == nr && _alt_setting == alt;
		}

		static bool type_matches(Xml_node const &node) {
			return node.has_type("interface"); }

		void update(Allocator &alloc, Xml_node const &node)
		{
			_active = node.attribute_value("active", false);
			_endpoints.update_from_xml(node,

				/* create */
				[&] (Xml_node const &node) -> Endpoint & {
					return *new (alloc) Endpoint(node, alloc, *this); },

				/* destroy */
				[&] (Endpoint &endp) {
					destroy(alloc, &endp); },

				/* update */
				[&] (Endpoint &, Xml_node const &) { }
			);
		}

		void update_urbs();

		template <typename FN>
		void with_endpoint(uint8_t index, FN const &fn)
		{
			_endpoints.for_each([&] (Endpoint &endp) {
				if (endp.address() == index) fn(endp);
			});
		}

		template <typename FN>
		void for_each_endpoint(FN const &fn) {
			_endpoints.for_each([&] (Endpoint &endp) { fn(endp); }); }
};


class Device : public List_model<Device>::Element
{
	public:

		using Name  = String<64>;
		using Speed = String<32>;

	private:

		Name                      const _name;
		Speed                     const _speed;
		Id_space<Device>::Element const _elem;
		Usb::Device                     _device;
		Signal_context_capability       _sigh_cap;
		USBHostDevice                  *_qemu_device { nullptr };
		List_model<::Interface>         _ifaces {};

		/**
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);

	public:

		struct Urb : Usb::Device::Urb
		{
			using Request_type =
				Usb::Device::Packet_descriptor::Request_type::access_t;

			USBPacket * const _packet;

			Urb(Device    &device,
			    uint8_t    request,
			    uint8_t    request_type,
			    uint16_t   value,
			    uint16_t   index,
			    size_t     size,
			    USBPacket *packet)
			:
				Usb::Device::Urb(device._device, request,
				                 (Request_type)request_type,
				                 value, index, size),
				_packet(packet) { }
		};

		Device(Name                     &name,
		       Speed                    &speed,
		       Usb::Connection          &usb,
		       Allocator                &alloc,
		       Region_map               &rm,
		       Id_space<Device>         &space,
		       Signal_context_capability cap)
		:
			_name(name),
			_speed(speed),
			_elem(*this, space),
			_device(usb, alloc, rm, name),
			_sigh_cap(cap)
		{
			_device.sigh(_sigh_cap);
		}

		~Device();

		Usb::Device &session() { return _device; }

		Name name() { return _name; }

		int speed()
		{
			if (_speed == "low")            return USB_SPEED_LOW;
			if (_speed == "full")           return USB_SPEED_FULL;
			if (_speed == "high")           return USB_SPEED_HIGH;
			if (_speed == "super")          return USB_SPEED_SUPER;
			if (_speed == "super_plus")     return USB_SPEED_SUPER;
			if (_speed == "super_plus_2x2") return USB_SPEED_SUPER;
			return USB_SPEED_FULL;
		}

		Signal_context_capability sigh_cap() { return _sigh_cap; }

		handle_t handle() const { return _elem.id().value; }

		void  qemu_device(USBHostDevice *dev) { _qemu_device = dev;  }
		USBHostDevice* qemu_device() { return _qemu_device; }

		bool matches(Xml_node const &node) const {
			return _name == node.attribute_value("name", Name()); }

		static bool type_matches(Xml_node const &node) {
			return node.has_type("device"); }

		void update(Allocator &alloc, Xml_node const &node)
		{
			Xml_node active_config = node;

			node.for_each_sub_node("config", [&] (Xml_node const &node) {
				if (node.attribute_value("active", false))
					active_config = node;
			});

			_ifaces.update_from_xml(active_config,

				/* create */
				[&] (Xml_node const &node) -> ::Interface & {
					return *new (alloc) ::Interface(*this, node); },

				/* destroy */
				[&] (::Interface &iface) {
					/* first clean up urbs before isoc-caches get destroyed */
					iface.destroy_all_urbs();
					iface.update(alloc, Xml_node("<empty/>"));
					destroy(alloc, &iface); },

				/* update */
				[&] (::Interface &iface, Xml_node const &node) {
					iface.update(alloc, node); }
			);
		}

		void update_urbs();

		template <typename FN>
		void with_active_interfaces(FN const &fn)
		{
			_ifaces.for_each([&] (::Interface &iface) {
				if (iface.active()) fn(iface); });
		}
};


struct Session
{
	Env                      &_env;
	Allocator                &_alloc;
	Signal_context_capability _handler_cap;
	Usb::Connection           _usb { _env };
	List_model<Device>        _model {};
	Id_space<Device>          _space {};
	Registry<Urb>             _urb_registry {};

	Session(Env &env, Allocator &alloc,
	        Signal_context_capability io_handler_cap,
	        Signal_context_capability rom_handler_cap)
	:
		_env(env), _alloc(alloc), _handler_cap(io_handler_cap)
	{
		_usb.sigh(rom_handler_cap);
	}

	~Session() {
		_model.for_each([&] (Device &dev) { destroy(_alloc, &dev); }); }

	void update()
	{
		_usb.with_xml([&] (Xml_node node) {
			_model.update_from_xml(node,

				/* create */
				[&] (Xml_node const &node) -> Device &
				{
					Device::Name name =
						node.attribute_value("name", Device::Name());
					Device::Speed speed =
						node.attribute_value("speed", Device::Speed());
					Device &dev = *new (_alloc)
						Device(name, speed, _usb, _alloc, _env.rm(),
						       _space, _handler_cap);
					return dev;
				},

				/* destroy */
				[&] (Device &dev)
				{
					if (dev.qemu_device())
						remove_usbdevice(dev.qemu_device());
					dev.update(_alloc, Xml_node("<empty/>"));
					destroy(_alloc, &dev);
				},

				/* update */
				[&] (Device &dev, Xml_node const &node)
				{
					dev.update(_alloc, node);
				}
			);
		});
		/* add new devices for C-API client after it got successfully added */
		_model.for_each([&] (Device &dev) {
			if (!dev.qemu_device())
				dev.qemu_device(create_usbdevice((void*)dev.handle(),
				                                 dev.speed()));
		});
	}
};


static Constructible<::Session>& _usb_session()
{
	static Constructible<::Session> session {};
	return session;
}


Urb::Urb(Registry<Urb> &registry,
         ::Interface   &iface,
         ::Endpoint    &endp,
         uint8_t        type,
         size_t         size,
         USBPacket     *packet)
:
	Registry<Urb>::Element(registry, *this),
	Usb::Endpoint(endp.address(), endp.attributes()),
	Usb::Interface::Urb(iface._session(), *this,
	                    _type(type), size),
	_endpoint(endp), _packet(packet) { }


Urb::Urb(Registry<Urb> &registry,
         ::Interface   &iface,
         ::Endpoint    &endp,
         uint8_t        type,
         size_t         size,
         uint32_t       isoc_packets)
:
	Registry<Urb>::Element(registry, *this),
	Usb::Endpoint(endp.address(), endp.attributes()),
	Usb::Interface::Urb(iface._session(), *this,
	                    _type(type), size, isoc_packets),
	_endpoint(endp) {}


size_t Urb::read_cache(Byte_range_ptr &dst)
{
	return _canceled ? 0 : _endpoint.read_cache(dst);
}


void Urb::write_cache(Const_byte_range_ptr const &src)
{
	if (!_canceled) _endpoint.write_cache(src);
}


void Urb::destroy()
{
	_endpoint.destroy_urb(this);
}



uint8_t Isoc_cache::_level() const {
	return (_ep.in()) ? _read-_wrote : _wrote-_read; }


void Isoc_cache::_new_urb()
{
	uint8_t urbs = (_ep.in()) ? URBS : _level() / PACKETS_PER_URB;

	bool sent = false;

	for (unsigned i = 0; urbs && i < URBS; i++) {
		if (_urbs[i].constructed())
			continue;
		_urbs[i].construct(_usb_session()->_urb_registry,
		                   _iface, _ep,
		                   USB_ENDPOINT_XFER_ISOC,
		                   _ep.max_packet_size()*PACKETS_PER_URB,
		                   PACKETS_PER_URB);
		--urbs;
		sent = true;
	}

	if (sent) _iface.update_urbs();
}


void Isoc_cache::_copy_to_host(USBPacket *p)
{
	size_t size = p->iov.size;

	if (!size || _level() >= MAX_PACKETS-1)
		return;

	size_t offset = _wrote * _ep.max_packet_size();

	if (size > _ep.max_packet_size()) {
		error("Assumption about QEmu Isochronous out packets wrong!");
		size = _ep.max_packet_size();
	}


	usb_packet_copy(p, _buffer+offset, size);
	_sizes[_wrote] = size;
	_wrote++;
}


void Isoc_cache::_copy_to_guest(USBPacket *p)
{
	size_t size = p->iov.size;

	while (size && _level()) {
		size_t offset = _read * _ep.max_packet_size();
		if (size < _sizes[_read])
			return;
		size -= _sizes[_read];
		usb_packet_copy(p, _buffer+offset, _sizes[_read++]);
	}
}


void Isoc_cache::handle(USBPacket *p)
{
	if (_ep.in()) _copy_to_guest(p);
	else          _copy_to_host(p);
	_new_urb();
}


size_t Isoc_cache::read(Byte_range_ptr &dst)
{
	if (_ep.in())
		return _ep.max_packet_size();

	size_t offset = _read * _ep.max_packet_size();
	Genode::memcpy(dst.start, (void*)(_buffer+offset), _sizes[_read]);
	return _sizes[_read++];
}


void Isoc_cache::write(Const_byte_range_ptr const &src)
{
	size_t offset = _wrote * _ep.max_packet_size();
	_sizes[_wrote++] = src.num_bytes;
	Genode::memcpy((void*)(_buffer+offset), src.start, src.num_bytes);
}


void Isoc_cache::destroy(Urb * urb)
{
	for (unsigned i = 0; i < URBS; i++)
		if (_urbs[i].constructed() && &*_urbs[i] == urb) {
			_urbs[i].destruct();
			return;
		}
	_new_urb();
}


void Isoc_cache::flush()
{
	_read  = 0;
	_wrote = 0;
	for (unsigned i = 0; i < URBS; i++)
		if (_urbs[i].constructed())
			_urbs[i]->cancel();
}


Isoc_cache::Isoc_cache(::Interface &iface, Endpoint &ep, Allocator &alloc)
:
	_iface(iface), _ep(ep), _alloc(alloc),
	_buffer((unsigned char*)_alloc.alloc(MAX_PACKETS*ep.max_packet_size())) {}


Usb::Interface &::Interface::_session()
{
	if (!_iface.constructed()) {
		_iface.construct(_device.session(),
		                 Usb::Interface::Index{_number, _alt_setting},
		                 _buf_size);
		_iface->sigh(_device.sigh_cap());
	}
	return *_iface;
};


#define USB_HOST_DEVICE(obj) \
        OBJECT_CHECK(USBHostDevice, (obj), TYPE_USB_HOST_DEVICE)


static void usb_host_update_ep(USBDevice *udev)
{
	USBHostDevice *d = USB_HOST_DEVICE(udev);
	handle_t handle = (handle_t)d->data;

	usb_ep_reset(udev);
	_usb_session()->_space.apply<Device>({ handle },
	                                     [&] (Device & device) {
		device.with_active_interfaces([&] (::Interface &iface) {
			iface.for_each_endpoint([&] (Endpoint &endp) {
				int     const pid   = (endp.address() & USB_DIR_IN)
					? USB_TOKEN_IN : USB_TOKEN_OUT;
				int     const ep    = (endp.address() & 0xf);
				uint8_t const type  = (endp.attributes() & 0x3);
				usb_ep_set_max_packet_size(udev, pid, ep, endp.max_packet_size());
				usb_ep_set_type(udev, pid, ep, type);
				usb_ep_set_ifnum(udev, pid, ep, iface.number());
				usb_ep_set_halted(udev, pid, ep, 0);
			});
		});
	});
}


void produce_out_data(USBPacket * const p, Byte_range_ptr &dst)
{
	USBEndpoint *ep   = p  ? p->ep   : nullptr;
	USBDevice   *udev = ep ? ep->dev : nullptr;

	if (!udev)
		return;

	switch (usb_ep_get_type(udev, p->pid, p->ep->nr)) {
	case USB_ENDPOINT_XFER_CONTROL:
		Genode::memcpy(dst.start, udev->data_buf, dst.num_bytes);
		return;
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		usb_packet_copy(p, dst.start, dst.num_bytes);
		break;
	default:
		error("cannot produce data for unknown packet");
	}
}


void consume_in_data(USBPacket * const p, Const_byte_range_ptr const &src)
{
	USBEndpoint *ep   = p  ? p->ep   : nullptr;
	USBDevice   *udev = ep ? ep->dev : nullptr;

	if (!udev)
		return;

	switch (usb_ep_get_type(udev, p->pid, p->ep->nr)) {
	case USB_ENDPOINT_XFER_CONTROL:
		p->actual_length = src.num_bytes;
		Genode::memcpy(udev->data_buf, src.start, src.num_bytes);

		/*
		 * Disable remote wakeup (bit 5) in 'bmAttributes' (byte 7) in reported
		 * configuration descriptor. On some systems (e.g., Windows) this will
		 * cause devices to stop working.
		 */
		if ((udev->setup_buf[1] == USB_REQ_GET_DESCRIPTOR) &&
		    (udev->setup_buf[3] == USB_DT_CONFIG))
			udev->data_buf[7] &= ~USB_CFG_ATT_WAKEUP;
		return;
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		/*
		 * unfortunately usb_packet_copy does not provide a signature
		 * for const-access of the source
		 */
		usb_packet_copy(p, const_cast<char*>(src.start), src.num_bytes);
		break;
	default:
		error("cannot consume data of unknown packet");
	}
}


void complete_packet(USBPacket * const p, Usb::Tagged_packet::Return_value v)
{
	USBEndpoint *ep   = p  ? p->ep   : nullptr;
	USBDevice   *udev = ep ? ep->dev : nullptr;

	if (!udev)
		return;

	bool ok = v == Usb::Tagged_packet::OK;

	switch (v) {
	case Usb::Tagged_packet::OK:   p->status = USB_RET_SUCCESS; break;
	case Usb::Tagged_packet::HALT: p->status = USB_RET_STALL;   break;
	default:                       p->status = USB_RET_IOERROR;
	};

	switch (usb_ep_get_type(udev, p->pid, p->ep->nr)) {
	case USB_ENDPOINT_XFER_CONTROL:
		if (ok && udev->setup_buf[1] == USB_REQ_SET_INTERFACE) {
			usb_host_update_devices();
			usb_host_update_ep(udev);
		}
		usb_generic_async_ctrl_complete(udev, p);
		return;
	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		usb_packet_complete(udev, p);
		break;
	default:
		error("cannot produce data for unknown packet");
	}
}


void ::Interface::update_urbs()
{
	if (!_iface.constructed())
		return;

	_iface->update_urbs<Urb>(
		[&] (Urb &urb, Byte_range_ptr &dst) {
			if (!urb.canceled()) produce_out_data(urb._packet, dst); },
		[&] (Urb &urb, Const_byte_range_ptr const &src) {
			if (!urb.canceled()) consume_in_data(urb._packet, src); },
		[&] (Urb &urb, uint32_t, Byte_range_ptr &dst) {
			return urb.read_cache(dst); },
		[&] (Urb &urb, uint32_t, Const_byte_range_ptr const &src) {
			urb.write_cache(src); },
		[&] (Urb &urb, Usb::Tagged_packet::Return_value v)
		{
			if (!urb.isoc() && !urb.canceled()) {
				complete_packet(urb._packet, v);
				if (_usb_session().constructed())
					destroy(_usb_session()->_alloc, &urb);
			} else
				urb.destroy();
		}
	);
}


void ::Interface::destroy_all_urbs()
{
	if (_iface.constructed())
		_iface->dissolve_all_urbs<Urb>([] (Urb &urb)
		{
			if (!urb.isoc() && !urb.canceled()) {
				complete_packet(urb._packet, Usb::Tagged_packet::NO_DEVICE);
				if (_usb_session().constructed())
					destroy(_usb_session()->_alloc, &urb);
			} else
				urb.destroy();
		});
}


void Device::update_urbs()
{
	_device.update_urbs<Urb>(
		[&] (Urb &urb, Byte_range_ptr &dst) {
			produce_out_data(urb._packet, dst); },
		[&] (Urb &urb, Const_byte_range_ptr const &src) {
			consume_in_data(urb._packet, src); },
		[&] (Urb &urb, Usb::Tagged_packet::Return_value v) {
			complete_packet(urb._packet, v);
			if (_usb_session().constructed())
				destroy(_usb_session()->_alloc, &urb);
		}
	);

	_ifaces.for_each([&] (::Interface &iface) {
		iface.update_urbs(); });
}


Device::~Device()
{
	_device.dissolve_all_urbs<Urb>([] (Urb &urb)
	{
		complete_packet(urb._packet, Usb::Tagged_packet::NO_DEVICE);
		if (_usb_session().constructed())
			destroy(_usb_session()->_alloc, &urb);
	});
}


extern "C" void usb_host_update_device_transfers()
{
	if (!_usb_session().constructed())
		return;

	_usb_session()->_model.for_each([&] (Device & device) {
		device.update_urbs(); });
}


static void usb_host_realize(USBDevice *udev, Error **errp)
{
	USBHostDevice *d = USB_HOST_DEVICE(udev);
	handle_t handle = (handle_t)d->data;

	udev->flags |= (1 << USB_DEV_FLAG_IS_HOST);
	usb_host_update_ep(udev);
}


static void usb_host_cancel_packet(USBDevice *udev, USBPacket *p)
{
	_usb_session()->_urb_registry.for_each([&] (::Urb &urb) {
		if (urb.packet() == p) urb.cancel(); });
}


static void usb_host_handle_data(USBDevice *udev, USBPacket *p)
{
	USBHostDevice *d = USB_HOST_DEVICE(udev);
	handle_t handle = (handle_t)d->data;
	uint8_t type = usb_ep_get_type(udev, p->pid, p->ep->nr);
	uint8_t ep   = p->ep->nr | ((p->pid == USB_TOKEN_IN) ? USB_DIR_IN : 0);

	_usb_session()->_space.apply<Device>({ handle }, [&] (Device & device) {
		device.with_active_interfaces([&] (::Interface &iface) {
			iface.with_endpoint(ep, [&] (Endpoint &endp) {

				switch (type) {
				case USB_ENDPOINT_XFER_BULK: [[fallthrough]];
				case USB_ENDPOINT_XFER_INT:
					p->status = USB_RET_ASYNC;
					new (_usb_session()->_alloc)
						::Urb(_usb_session()->_urb_registry,
						      iface, endp, type, usb_packet_size(p), p);
					iface.update_urbs();
					return;
				case USB_ENDPOINT_XFER_ISOC:
					p->status = USB_RET_SUCCESS;
					endp.handle_isoc_packet(p);
					return;
				default:
					error("not supported data request ", (int)type);
					p->status = USB_RET_NAK;
					return;
				}
			});
		});
	});
}


static void usb_host_handle_control(USBDevice *udev, USBPacket *p,
                                    int request, int value, int index,
                                    int length, uint8_t *data)
{
	USBHostDevice *d = USB_HOST_DEVICE(udev);
	handle_t handle = (handle_t)d->data;

	switch (request) {
	case DeviceOutRequest | USB_REQ_SET_ADDRESS:
		udev->addr = value;
		p->status = USB_RET_SUCCESS;
		return;
	}

	if (udev->speed == USB_SPEED_SUPER &&
		!(udev->port->speedmask & USB_SPEED_MASK_SUPER) &&
		request == 0x8006 && value == 0x100 && index == 0) {
		error("r->usb3ep0quirk = true");
	}

	_usb_session()->_space.apply<Device>({ handle },
	                                     [&] (Device & device) {
		new (_usb_session()->_alloc)
			Device::Urb(device, request & 0xff, (request >> 8) & 0xff,
			            value, index, length, p);
		device.update_urbs();
	});

	p->status = USB_RET_ASYNC;
}


static void usb_host_ep_stopped(USBDevice *udev, USBEndpoint *usb_ep)
{
	USBHostDevice *d = USB_HOST_DEVICE(udev);
	handle_t handle = (handle_t)d->data;
	uint8_t ep = usb_ep->nr | ((usb_ep->pid == USB_TOKEN_IN) ? USB_DIR_IN : 0);

	_usb_session()->_space.apply<Device>({ handle }, [&] (Device & device) {
		device.with_active_interfaces([&] (::Interface &iface) {
			iface.with_endpoint(ep, [&] (Endpoint &endp) {
				endp.flush();
				new (_usb_session()->_alloc)
					::Urb(_usb_session()->_urb_registry, iface, endp,
					      USB_ENDPOINT_XFER_INVALID, 0, nullptr);
			});
		});
	});
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


extern "C" void usb_host_update_devices()
{
	if (_usb_session().constructed()) _usb_session()->update();
}


extern "C" void usb_host_destroy() { }


/*
 * Do not use type_init macro because of name mangling
 */
extern "C" void _type_init_usb_host_register_types(Entrypoint *ep,
                                                   Allocator *alloc,
                                                   Env *env)
{
	struct Helper
	{
		Signal_handler<Helper> io_handler;
		Signal_handler<Helper> rom_handler;

		Helper(Entrypoint &ep)
		:
			io_handler(ep, *this, &Helper::io),
			rom_handler(ep, *this, &Helper::rom_update) {}

		void io()
		{
			Mutex::Guard guard(::_mutex);
			usb_host_update_device_transfers();
		}

		void rom_update()
		{
			Mutex::Guard guard(::_mutex);
			usb_host_update_devices();
		}
	};
	static Helper helper(*ep);

	Mutex::Guard guard(::_mutex);
	usb_host_register_types();
	_usb_session().construct(*env, *alloc, helper.io_handler,
	                         helper.rom_handler);
	usb_host_update_devices();
}
