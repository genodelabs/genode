/*
 * \brief  Genode USB service provider C-API
 * \author Stefan Kalkowski
 * \date   2021-09-14
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/quota_guard.h>
#include <base/session_object.h>
#include <root/component.h>
#include <os/dynamic_rom_session.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <packet_stream_tx/rpc_object.h>
#include <usb_session/usb_session.h>
#include <util/bit_allocator.h>

#include <genode_c_api/usb.h>

using namespace Genode;
using namespace Usb;


using String_item = String<64>;

static String_item string_item(genode_usb_dev_string_item_t func, void *opaque_data)
{
	char buf[String_item::capacity()] { };

	func({ buf, sizeof(buf) }, opaque_data);

	char *first = buf;
	char *last  = buf + sizeof(buf) - 1;

	/* skip leading whitespace */
	while (*first == ' ') ++first;

	/* trim trailing whitespace */
	while (last >= first && (*last == 0 || *last == ' '))
		--last;

	return { Cstring(first, last + 1 - first) };
}


/**
 * The API of the Registry class is favored in contrast to the ancient List,
 * but due to different potentially blocking user-level-scheduled threads,
 * e.g., within DDE Linux we cannot use a mutex-guarded registry.
 * Those threads are logical ones, and do not have real concurrency. Moreover,
 * only one and the same threads insert/remove/handle a single device.
 * Therefore, thread safety is not an issue.
 */
template <typename T>
struct Reg_list
{
	public:

		class Element : public List<Element>::Element
		{
			private:

				friend class Reg_list;

				Reg_list &_registry;
				T        &_object;

				/*
				 * Noncopyable
				 */
				Element(Element const &);
				Element &operator = (Element const &);

			public:

				Element(Reg_list &r, T &o);
				~Element();
		};

	private:

		List<Element> _elements { };

	public:

		template <typename FN>
		void for_each(FN const & fn)
		{
			Element *e = _elements.first(), *next = nullptr;
			for ( ; e; e = next) {
				next = e->next();
				fn(e->_object);
			}
		}

		template <typename FN>
		void for_each(FN const & fn) const
		{
			Element const *e = _elements.first(), *next = nullptr;
			for ( ; e; e = next) {
				next = e->next();
				fn(e->_object);
			}
		}
};


template <typename T> Reg_list<T>::Element::Element(Reg_list &r, T &o)
: _registry(r), _object(o) { r._elements.insert(this); }



template <typename T> Reg_list<T>::Element::~Element() {
	_registry._elements.remove(this); }


struct genode_usb_endpoint : Reg_list<genode_usb_endpoint>::Element
{
	genode_usb_endpoint_descriptor desc;

	genode_usb_endpoint(Reg_list<genode_usb_endpoint> &registry,
	                    genode_usb_endpoint_descriptor desc)
	:
		Reg_list<genode_usb_endpoint>::Element(registry, *this),
		desc(desc) {}
};


struct genode_usb_interface : Reg_list<genode_usb_interface>::Element
{
	String_item                     const info;
	genode_usb_interface_descriptor const desc;
	bool                                  active;
	Reg_list<genode_usb_endpoint>   endpoints {};

	genode_usb_interface(Reg_list<genode_usb_interface> &registry,
	                     String_item              const &info,
	                     genode_usb_interface_descriptor desc,
	                     bool                            active)
	:
		Reg_list<genode_usb_interface>::Element(registry, *this),
		info(info), desc(desc), active(active) {}
};


struct genode_usb_configuration : Reg_list<genode_usb_configuration>::Element
{
	genode_usb_config_descriptor const desc;
	bool                               active;
	Reg_list<genode_usb_interface>     interfaces {};

	genode_usb_configuration(Reg_list<genode_usb_configuration> &registry,
	                         genode_usb_config_descriptor        desc,
	                         bool                                active)
	:
		Reg_list<genode_usb_configuration>::Element(registry, *this),
		desc(desc), active(active) {}
};


struct genode_usb_device : Reg_list<genode_usb_device>::Element
{
	genode_usb_bus_num_t               const bus;
	genode_usb_dev_num_t               const dev;
	genode_usb_speed_t                 const speed;
	String_item                        const manufacturer;
	String_item                        const product;
	genode_usb_device_descriptor       const desc;
	Reg_list<genode_usb_configuration> configs {};

	genode_usb_device(Reg_list<genode_usb_device> &registry,
	                  genode_usb_bus_num_t         bus,
	                  genode_usb_dev_num_t         dev,
	                  genode_usb_speed_t           speed,
	                  String_item           const &manufacturer,
	                  String_item           const &product,
	                  genode_usb_device_descriptor desc)
	:
		Reg_list<genode_usb_device>::Element(registry, *this),
		bus(bus), dev(dev), speed(speed),
		manufacturer(manufacturer), product(product), desc(desc) {}

	using Label = String<64>;

	Label label() const { return Label("usb-", bus, "-", dev); }

	String<32> speed_to_string() const
	{
		switch (speed) {
		case GENODE_USB_SPEED_LOW:            return "low";
		case GENODE_USB_SPEED_FULL:           return "full";
		case GENODE_USB_SPEED_HIGH:           return "high";
		case GENODE_USB_SPEED_SUPER:          return "super";
		case GENODE_USB_SPEED_SUPER_PLUS:     return "super_plus";
		case GENODE_USB_SPEED_SUPER_PLUS_2X2: return "super_plus_2x2";
		};

		return "full";
	}

	void generate(Xml_generator & xml, bool acquired) const;
};


struct Dma_allocator : Interface
{
	virtual genode_shared_dataspace * alloc_dma_dataspace(size_t size) = 0;

	virtual void free_dma_dataspace(genode_shared_dataspace * ds, size_t size) = 0;
};


template <typename SESSION, typename T>
class Packet_handler
:
	public Genode::Rpc_object<SESSION, T>,
	private Reg_list<T>::Element
{
	protected:

		enum State { CONNECTED, DISCONNECTED };

		using Packet_descriptor = typename SESSION::Packet_descriptor;
		using Tx = typename SESSION::Tx;
		static constexpr size_t MAX_PACKETS = SESSION::TX_QUEUE_SIZE;

		Env                             &_env;
		Dma_allocator                   &_alloc;
		State                            _state { CONNECTED };
		size_t                           _buf_size;
		genode_shared_dataspace         &_ds;
		Packet_stream_tx::Rpc_object<Tx> _tx;

		Constructible<Packet_descriptor> _packets[MAX_PACKETS] { };

		Capability<SESSION> _cap;

		enum  Index_error  { OUT_OF_BOUNDS };
		using Index_result = Attempt<size_t, Index_error>;

		Index_result _idx_avail()
		{
			size_t idx = 0;
			for (auto const &p : _packets) {
				if (!p.constructed())
					break;
				++idx;
			}
			if (idx == MAX_PACKETS)
				return OUT_OF_BOUNDS;
			return idx;
		}

		bool _packet_avail()
		{
			unsigned packets_in_flight = 0;
			for (auto const &p : _packets)
				if (p.constructed()) packets_in_flight++;

			return _tx.sink()->packet_avail() &&
			       (_tx.sink()->ack_slots_free() > packets_in_flight);
		}

		template <typename FN>
		void _for_each_packet(FN const & fn)
		{
			while (true) {
				size_t idx = ~0UL;
				_idx_avail().with_result([&] (size_t i) { idx = i; },
				                         [&] (Index_error) {});

				if (idx > MAX_PACKETS || !_packet_avail())
					break;

				Packet_descriptor p = _tx.sink()->try_get_packet();
				if (!_tx.sink()->packet_valid(p))
					break;

				_packets[idx].construct(p);
				fn(_packets[idx]);
			}
		}

		enum  Packet_error { NO_PACKET };
		using Finish_packet_result = Attempt<Packet_descriptor, Packet_error>;

		Finish_packet_result _finish_packet(genode_usb_request_handle_t handle)
		{
			if ((addr_t)handle < (addr_t)&_packets ||
			    (addr_t)handle > ((addr_t)&_packets + sizeof(_packets)))
				return NO_PACKET;

			Constructible<Packet_descriptor> & cp =
				*reinterpret_cast<Constructible<Packet_descriptor>*>(handle);
			if (!cp.constructed())
				return NO_PACKET;

			Packet_descriptor p = *cp;
			cp.destruct();
			return p;
		}

		void _ack(Packet_descriptor::Return_value v,
		          size_t                          actual_size,
                  Packet_descriptor               p)
		{
			p.return_value        = v;
			p.payload_return_size = actual_size;
			if (!_tx.sink()->try_ack_packet(p))
				error("USB client's ack queue run full, looses packet ack!");
		}

		virtual void
		_handle_request(Constructible<Packet_descriptor> &cp,
		                genode_buffer_t                   payload,
		                genode_usb_req_callback_t const   callback,
		                void                             *opaque_data) = 0;
	public:

		Packet_handler(Env                       &env,
		               Dma_allocator             &alloc,
		               Reg_list<T>               &registry,
		               size_t                     buf_size,
		               T                         &object,
		               Signal_context_capability  sigh_cap)
		:
			Reg_list<T>::Element(registry, object),
			_env(env),
			_alloc(alloc),
			_buf_size(buf_size),
			_ds(*alloc.alloc_dma_dataspace(buf_size)),
			_tx(genode_shared_dataspace_capability(&_ds), env.rm(),
			    env.ep().rpc_ep()),
			_cap(env.ep().rpc_ep().manage(this))
		{
			_tx.sigh_packet_avail(sigh_cap);
			_tx.sigh_ready_to_ack(sigh_cap);
		}


		~Packet_handler()
		{
			_env.ep().rpc_ep().dissolve(this);

			if (_tx.dataspace().valid())
				_alloc.free_dma_dataspace(&_ds, _buf_size);
		}

		Capability<SESSION> session_cap() { return _cap; }
		Capability<Tx> tx_cap() { return _tx.cap(); }

		bool connected() const { return _state == CONNECTED; }

		virtual void disconnect() { _state = DISCONNECTED; }

		virtual void wakeup() { _tx.sink()->wakeup(); }

		virtual bool request(genode_usb_req_callback_t const callback,
                             void                           *opaque_data)
		{
			bool ret = false;
			_for_each_packet([&] (Constructible<Packet_descriptor> &cp) {
				void * addr = _tx.sink()->packet_content(*cp);
				if (addr)
					addr = (void*)(genode_shared_dataspace_local_address(&_ds) +
					               ((addr_t)addr -
					                (addr_t)_tx.sink()->ds_local_base()));
				genode_buffer_t buf { addr, addr ? cp->size() : 0 };
				_handle_request(cp, buf, callback, opaque_data);
				ret = true;
			});
			return ret;
		}

		virtual void handle_disconnected()
		{
			if (_state != DISCONNECTED)
				return;

			while (_packet_avail()) {
				Packet_descriptor p = _tx.sink()->try_get_packet();
				if (_tx.sink()->packet_valid(p))
					_ack(Packet_descriptor::NO_DEVICE, 0, p);
			}
		}
};


class Session_component;

class Interface_component
: public Packet_handler<Interface_session, Interface_component>
{
	private:

		using Base = Packet_handler<Interface_session, Interface_component>;

		friend class Device_component;

		enum { MAX_EPS = 30, UNUSED = 255 };

		uint8_t _iface_idx;
		uint8_t _ep_addresses[MAX_EPS] { UNUSED };

		void
		_handle_request(Constructible<Packet_descriptor> &cp,
		                genode_buffer_t                   payload,
		                genode_usb_req_callback_t const   callback,
		                void                             *opaque_data) override;

	public:

		Interface_component(Env                           &env,
		                    Reg_list<Interface_component> &registry,
		                    Session_component             &session,
		                    genode_usb_device::Label       label,
		                    size_t                         buf_size,
		                    Signal_context_capability      sigh_cap,
		                    uint8_t                        iface_idx);
		Interface_component(Env                           &env,
		                    Reg_list<Interface_component> &registry,
		                    Session_component             &session,
		                    size_t                         buf_size,
		                    Signal_context_capability      sigh_cap);

		bool handle_response(genode_usb_request_handle_t request_id,
		                     genode_usb_request_ret_t    return_value,
		                     uint32_t                   *actual_sizes);
};


class Device_component
: public Packet_handler<Device_session, Device_component>
{
	private:

		using Base = Packet_handler<Device_session, Device_component>;

		friend class Session_component;

		Env                             &_env;
		Heap                            &_heap;
		Session_component               &_session;
		bool                       const _controls;
		genode_usb_device::Label   const _device_label;
		Reg_list<Interface_component>    _interfaces {};
		Signal_context_capability        _sigh_cap;
		bool                             _warn_once { true };

		void
		_handle_request(Constructible<Packet_descriptor> &cp,
		                genode_buffer_t                   payload,
		                genode_usb_req_callback_t const   callback,
		                void                             *opaque_data) override;

	public:

		Device_component(Env                            &env,
		                 Heap                           &heap,
		                 Reg_list<Device_component>     &registry,
		                 Session_component              &session,
		                 bool                     const  controls,
		                 genode_usb_device::Label const &device,
		                 Signal_context_capability       sigh_cap);
		Device_component(Env                            &env,
		                 Heap                           &heap,
		                 Reg_list<Device_component>     &registry,
		                 Session_component              &session,
		                 bool                     const  controls,
		                 Signal_context_capability       sigh_cap);
		~Device_component();

		bool request(genode_usb_req_callback_t const callback,
		             void                           *opaque_data) override;
		bool handle_response(genode_usb_request_handle_t request_id,
		                     genode_usb_request_ret_t    return_value,
		                     uint32_t                   *actual_sizes);
		void disconnect() override;
		void wakeup() override;
		void handle_disconnected() override;


		/******************************
		 ** Device_session interface **
		 ******************************/

		Interface_capability acquire_interface(uint8_t index, size_t buf_size);
		void release_interface(Interface_capability cap);
};

class Root;

class Session_component
:
	public  Dma_allocator,
	public  Session_object<Usb::Session>,
	private Reg_list<Session_component>::Element,
	private Dynamic_rom_session::Xml_producer
{
	private:

		friend class Device_component;

		Env                                   &_env;
		::Root                                &_root;
		Reg_list<Session_component>           &_sessions;
		Reg_list<genode_usb_device>           &_devices;
		Attached_rom_dataspace                &_config;
		Signal_context_capability              _sigh_cap;
		genode_shared_dataspace_alloc_attach_t _alloc_fn;
		genode_shared_dataspace_free_t         _free_fn;
		genode_usb_dev_release_t               _release_fn;

		Constrained_ram_allocator  _env_ram     { _env.pd(),
		                                          _ram_quota_guard(),
		                                          _cap_quota_guard()  };
		Heap                       _heap        { _env_ram, _env.rm() };
		Dynamic_rom_session        _rom_session { _env.ep(), _env_ram,
		                                          _env.rm(), *this    };

		Reg_list<Device_component> _device_sessions {};

		enum State { ACTIVE, IN_DESTRUCTION } _state { ACTIVE };

		/*
		 * Non_copyable
		 */
		Session_component(const Session_component&);
		Session_component & operator=(const Session_component&);

		template <typename FN>
		void _device_policy(genode_usb_device const & d, FN const &fn);

		bool _matches(genode_usb_device const & device);

		Device_capability _acquire(genode_usb_device::Label const &device,
		                           bool const controls);
		void _release(Device_component &dc);

	public:

		Session_component(Env                                   &env,
		                  ::Root                                &root,
		                  Reg_list<Session_component>           &registry,
		                  Reg_list<genode_usb_device>           &devices,
		                  Attached_rom_dataspace                &config,
		                  Signal_context_capability              sigh_cap,
		                  genode_shared_dataspace_alloc_attach_t alloc_fn,
		                  genode_shared_dataspace_free_t         free_fn,
		                  genode_usb_dev_release_t               release_fn,
		                  Label     const                       &label,
		                  Resources const                       &resources,
		                  Diag      const                       &diag);

		~Session_component();

		void announce_device(genode_usb_device const & device);
		void discontinue_device(genode_usb_device const & device);
		void update_policy();
		void update_devices_rom();

		bool acquired(genode_usb_device const &dev);
		bool request(genode_usb_device         const &dev,
		             genode_usb_req_callback_t const  callback,
		             void                            *opaque_data);
		bool handle_response(genode_usb_request_handle_t request_id,
		                     genode_usb_request_ret_t    return_value,
		                     uint32_t                   *actual_sizes);
		void handle_disconnected();
		void wakeup();

		bool matches(genode_usb_device::Label label, uint8_t iface_idx);

		template <typename FN>
		void for_each_ep(genode_usb_device::Label label,
		                 uint8_t iface_idx, FN const &fn);

		void set_interface(genode_usb_device::Label label,
		                   uint16_t num, uint16_t alt);
		void set_configuration(genode_usb_device::Label label,
		                       uint16_t num);

		/*****************************
		 ** Dma_allocator interface **
		 *****************************/

		genode_shared_dataspace * alloc_dma_dataspace(size_t size) override
		{
			Ram_quota const needed_ram { size + 4096 };

			if (!_ram_quota_guard().have_avail(needed_ram))
				throw Out_of_ram();

			_cap_quota_guard().replenish(Cap_quota{2});
			_ram_quota_guard().replenish(needed_ram);

			return _alloc_fn(size);
		}

		void free_dma_dataspace(genode_shared_dataspace *ds, size_t size) override
		{
			_cap_quota_guard().replenish(Cap_quota{2});
			_ram_quota_guard().replenish(Ram_quota{size+4096});
			_free_fn(ds);
		}


		/***************************
		 ** USB session interface **
		 ***************************/

		Rom_session_capability devices_rom() override;
		Device_capability acquire_device(Device_name const &name) override;
		Device_capability acquire_single_device() override;
		void release_device(Device_capability) override;


		/*******************************************
		 ** Dynamic_rom_session::Xml_producer API **
		 *******************************************/

		void produce_xml(Xml_generator &xml) override;
};


class Root : Sliced_heap, public Root_component<Session_component>
{
	private:

		Env                        &_env;
		Heap                        _heap { _env.ram(), _env.rm() };
		Signal_context_capability   _sigh_cap;
		Attached_rom_dataspace      _config   { _env, "config"  };
		Signal_handler<Root>        _config_handler { _env.ep(), *this,
		                                              &Root::_config_update };
		Reporter                    _config_reporter { _env, "config"  };
		Reg_list<genode_usb_device> _devices {};
		bool                        _announced { false };

		Constructible<Expanding_reporter> _device_reporter {};
		Reg_list<Session_component>       _sessions {};

		genode_shared_dataspace_alloc_attach_t _alloc_fn;
		genode_shared_dataspace_free_t         _free_fn;
		genode_usb_dev_release_t               _release_fn;

		Root(const Root&);
		Root & operator=(const Root&);

		Session_component * _create_session(const char * args,
		                                     Affinity const &) override;
		void _upgrade_session(Session_component *, const char *) override;

		void _config_update();
		void _announce_service();

	public:

		Root(Env                                   &env,
		     Signal_context_capability              sigh_cap,
		     genode_shared_dataspace_alloc_attach_t alloc_fn,
		     genode_shared_dataspace_free_t         free_fn,
		     genode_usb_dev_release_t               release_fn);

		/*
		 * Update report about existing USB devices
		 */
		void report();

		void device_add_endpoint(struct genode_usb_interface   *iface,
		                         genode_usb_endpoint_descriptor desc);
		void device_add_interface(struct genode_usb_configuration *cfg,
		                          genode_usb_dev_string_item_t     info_string,
		                          genode_usb_interface_descriptor  desc,
		                          genode_usb_dev_add_endp_t        callback,
		                          void                            *opaque_data,
		                          bool                             active);
		void device_add_configuration(struct genode_usb_device    *dev,
		                              genode_usb_config_descriptor desc,
		                              genode_usb_dev_add_iface_t   callback,
		                              void                        *opaque_data,
		                              bool                         active);
		void announce_device(genode_usb_bus_num_t         bus,
		                     genode_usb_dev_num_t         dev,
		                     genode_usb_speed_t           speed,
		                     genode_usb_dev_string_item_t manufacturer_string,
		                     genode_usb_dev_string_item_t product_string,
		                     genode_usb_device_descriptor desc,
		                     genode_usb_dev_add_config_t  callback,
		                     void                        *opaque_data);

		void discontinue_device(genode_usb_bus_num_t bus,
		                        genode_usb_dev_num_t dev);

		bool acquired(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev);

		bool request(genode_usb_bus_num_t            bus,
		             genode_usb_dev_num_t            dev,
		             genode_usb_req_callback_t const callback,
		             void                           *opaque_data);

		void handle_response(genode_usb_request_handle_t request_id,
		                     genode_usb_request_ret_t    return_value,
		                     uint32_t                   *actual_sizes);

		/*
		 * Acknowledge requests from sessions with disconnected devices
		 */
		void handle_disconnected_sessions();

		void wakeup();
};


static ::Root * _usb_root  = nullptr;


void genode_usb_device::generate(Xml_generator & xml, bool acquired) const
{
	using Value = String<64>;

	auto per_endp = [&] (genode_usb_endpoint const & endp)
	{
		xml.node("endpoint", [&] {
			xml.attribute("address",    Value(Hex(endp.desc.address)));
			xml.attribute("attributes", Value(Hex(endp.desc.attributes)));
			xml.attribute("max_packet_size",
			              Value(Hex(endp.desc.max_packet_size)));
		});
	};

	auto per_iface = [&] (genode_usb_interface const & iface)
	{
		xml.node("interface", [&] {
			xml.attribute("active",      iface.active);
			xml.attribute("number",      Value(Hex(iface.desc.number)));
			if (*iface.info.string()) xml.attribute("info", iface.info);
			xml.attribute("alt_setting", Value(Hex(iface.desc.alt_settings)));
			xml.attribute("class",       Value(Hex(iface.desc.iclass)));
			xml.attribute("subclass",    Value(Hex(iface.desc.isubclass)));
			xml.attribute("protocol",    Value(Hex(iface.desc.iprotocol)));
			iface.endpoints.for_each(per_endp);
		});
	};

	auto per_config = [&] (genode_usb_configuration const & cfg)
	{
		xml.node("config", [&] {
			xml.attribute("active", cfg.active);
			xml.attribute("value",  Value(Hex(cfg.desc.config_value)));
			cfg.interfaces.for_each(per_iface);
		});
	};

	xml.node("device", [&] {
		xml.attribute("name",       label());
		xml.attribute("class",      Value(Hex(desc.dclass)));
		if (*manufacturer.string()) xml.attribute("manufacturer", manufacturer);
		if (*product.string())      xml.attribute("product", product);
		xml.attribute("vendor_id",  Value(Hex(desc.vendor_id)));
		xml.attribute("product_id", Value(Hex(desc.product_id)));
		xml.attribute("speed",      speed_to_string());
		if (acquired)               xml.attribute("acquired", true);
		configs.for_each(per_config);
	});
}


void
Interface_component::_handle_request(Constructible<Packet_descriptor> &cpd,
                                     genode_buffer_t                   payload,
                                     genode_usb_req_callback_t const   cbs,
                                     void                             *opaque_data)
{
	genode_usb_request_handle_t handle = &cpd;
	bool granted = false;

	for (unsigned i = 0; i < MAX_EPS && !granted; i++)
		if (_ep_addresses[i] == cpd->index) granted = true;

	if (!granted) {
		handle_response(handle, INVALID, nullptr);
		return;
	}

	switch (cpd->type) {
	case Packet_descriptor::BULK:
		cbs->bulk_fn(handle, cpd->index, payload, opaque_data);
		break;
	case Packet_descriptor::IRQ:
		cbs->irq_fn(handle, cpd->index, payload, opaque_data);
		break;
	case Packet_descriptor::ISOC:
		{
			genode_usb_isoc_transfer_header & hdr =
				*reinterpret_cast<genode_usb_isoc_transfer_header*>(payload.addr);
			genode_buffer_t isoc_payload {
				(void*) ((addr_t)payload.addr +
				         sizeof(genode_usb_isoc_transfer_header) +
				         (hdr.number_of_packets *
				          sizeof(genode_usb_isoc_descriptor))),
				payload.size - sizeof(genode_usb_isoc_transfer_header) +
				hdr.number_of_packets * sizeof(genode_usb_isoc_descriptor) };
			cbs->isoc_fn(handle, cpd->index, hdr.number_of_packets,
			             hdr.packets, isoc_payload, opaque_data);
			break;
		}
	case Packet_descriptor::FLUSH:
		cbs->flush_fn(cpd->index, handle, opaque_data);
		break;
	};
}


bool
Interface_component::handle_response(genode_usb_request_handle_t handle,
                                     genode_usb_request_ret_t    value,
                                     uint32_t                   *actual_sizes)
{
	bool ret = false;
	_finish_packet(handle).with_result(
		[&] (Packet_descriptor p) {
			ret = true;
			Packet_descriptor::Return_value v {};
			switch (value) {
			case OK:        v = Packet_descriptor::OK;        break;
			case NO_DEVICE: v = Packet_descriptor::NO_DEVICE; break;
			case INVALID:   v = Packet_descriptor::INVALID;   break;
			case HALT:      v = Packet_descriptor::HALT;      break;
			case TIMEOUT:
				error("timeout shouldn't be returned for transfer URBs");
			};

			/* return actual isoc frames sizes */
			if ((value == OK) &&
			    (p.type == Packet_descriptor::ISOC)) {
				void * data = _tx.sink()->packet_content(p);
				genode_usb_isoc_transfer_header & hdr =
					*reinterpret_cast<genode_usb_isoc_transfer_header*>(data);
				for (size_t i = 0; i < hdr.number_of_packets; i++)
					hdr.packets[i].actual_size = actual_sizes[i+1];
			}

			_ack(v, (value == OK) ? actual_sizes[0] : 0, p);
		},
		[&] (Packet_error) {});
	return ret;
}


Interface_component::Interface_component(Env                           &env,
                                         Reg_list<Interface_component> &registry,
                                         Session_component             &session,
                                         genode_usb_device::Label       label,
                                         size_t                         buf_size,
                                         Signal_context_capability      sigh_cap,
                                         uint8_t                        iface_idx)
:
	Base(env, session, registry, buf_size, *this, sigh_cap),
	_iface_idx(iface_idx)
{
	unsigned idx = 0;
	session.for_each_ep(label, iface_idx, [&] (genode_usb_endpoint const &ep) {
		if (idx < MAX_EPS) _ep_addresses[idx++] = ep.desc.address; });
}


Interface_component::Interface_component(Env                           &env,
                                         Reg_list<Interface_component> &registry,
                                         Session_component             &session,
                                         size_t                         buf_size,
                                         Signal_context_capability      sigh_cap)
:
	Base(env, session, registry, buf_size, *this, sigh_cap),
	_iface_idx(0xff)
{
	disconnect();
}


void
Device_component::_handle_request(Constructible<Packet_descriptor> &cpd,
                                  genode_buffer_t                   payload,
                                  genode_usb_req_callback_t const   cbs,
                                  void                             *opaque_data)
{
	using P = Packet_descriptor;

	genode_usb_request_handle_t handle = &cpd;
	bool granted = false;

	switch (cpd->request) {
	case P::GET_STATUS:        [[fallthrough]];
	case P::GET_DESCRIPTOR:    [[fallthrough]];
	case P::GET_CONFIGURATION: [[fallthrough]];
	case P::GET_INTERFACE:
		granted = true;
		break;
	case P::SET_INTERFACE:
		_interfaces.for_each([&] (Interface_component & ic) {
			if (ic._iface_idx == cpd->index) granted = true; });
		if (granted) break;
		[[fallthrough]];
	default: granted = _controls;
	};

	if (!granted) {
		uint32_t ret = 0;
		handle_response(handle, INVALID, &ret);
		if (_warn_once) {
			warning("Invalid restricted control URB to device ", _device_label,
			        " from session ", _session.label());
			_warn_once = false;
		}
		return;
	}

	cbs->ctrl_fn(handle, cpd->request, cpd->request_type, cpd->value,
	             cpd->index, cpd->timeout, payload, opaque_data);
}


Interface_capability Device_component::acquire_interface(uint8_t index,
                                                         size_t  buf_size)
{
	if (!_session.matches(_device_label, index))
		return (new (_heap) Interface_component(_env, _interfaces, _session,
		                                        buf_size, _sigh_cap))->session_cap();
	return (new (_heap)
		Interface_component(_env, _interfaces, _session, _device_label,
		                    buf_size, _sigh_cap, index))->session_cap();
}


void Device_component::release_interface(Interface_capability cap)
{
	if (!cap.valid())
		return;

	_interfaces.for_each([&] (Interface_component & ic) {
		if (cap.local_name() == ic.cap().local_name())
			destroy(_heap, &ic); });
}


bool Device_component::request(genode_usb_req_callback_t const callback,
                               void                           *opaque_data)
{
	bool ret = false;

	_interfaces.for_each([&] (Interface_component & ic) {
		if (ic.request(callback, opaque_data)) ret = true; });

	return ret ? ret : Base::request(callback, opaque_data);
}


bool
Device_component::handle_response(genode_usb_request_handle_t handle,
                                  genode_usb_request_ret_t    value,
                                  uint32_t                   *actual_sizes)
{
	bool ret = false;
	_finish_packet(handle).with_result(
		[&] (Packet_descriptor p) {
			ret = true;
			Packet_descriptor::Return_value v =
				Packet_descriptor::INVALID;
			switch (value) {
			case OK:        v = Packet_descriptor::OK;        break;
			case NO_DEVICE: v = Packet_descriptor::NO_DEVICE; break;
			case INVALID:   v = Packet_descriptor::INVALID;   break;
			case TIMEOUT:   v = Packet_descriptor::TIMEOUT;   break;
			case HALT:      v = Packet_descriptor::HALT;      break;
			};
			_ack(v, (value == OK) ? actual_sizes[0] : 0, p);

			if (value != OK)
				return;

			if (p.request == Packet_descriptor::SET_INTERFACE &&
			    p.request_type == Packet_descriptor::IFACE) {
				_session.set_interface(_device_label, p.index, p.value);
				return;
			}

			if (p.request == Packet_descriptor::SET_CONFIGURATION &&
			    p.request_type == Packet_descriptor::DEVICE) {
				_session.set_configuration(_device_label, p.value);
				return;
			}
		},
		[&] (Packet_error) {});

	if (!ret)
		_interfaces.for_each([&] (Interface_component & ic) {
			if (ret) return;
			if (ic.handle_response(handle, value, actual_sizes)) ret = true;
		});

	return ret;
}


void Device_component::disconnect()
{
	Base::disconnect();

	_interfaces.for_each([&] (Interface_component & ic) {
		ic.disconnect(); });
}


void Device_component::handle_disconnected()
{
	Base::handle_disconnected();

	_interfaces.for_each([&] (Interface_component & ic) {
		ic.handle_disconnected(); });
}


void Device_component::wakeup()
{
	Base::wakeup();

	_interfaces.for_each([&] (Interface_component & ic) {
		ic.wakeup(); });
}


Device_component::Device_component(Env                            &env,
                                   Heap                           &heap,
                                   Reg_list<Device_component>     &registry,
                                   Session_component              &session,
                                   bool                     const  controls,
                                   genode_usb_device::Label const &device,
                                   Signal_context_capability       sigh_cap)
:
	Packet_handler<Device_session, Device_component>(env, session, registry,
	                                                 TX_BUFFER_SIZE,
	                                                 *this, sigh_cap),
	_env(env),
	_heap(heap),
	_session(session),
	_controls(controls),
	_device_label(device),
	_sigh_cap(sigh_cap) {}


Device_component::Device_component(Env                            &env,
                                   Heap                           &heap,
                                   Reg_list<Device_component>     &registry,
                                   Session_component              &session,
                                   bool                     const  controls,
                                   Signal_context_capability       sigh_cap)
:
	Device_component(env, heap, registry, session, controls,
	                 "INVALID_DEVICE", sigh_cap)
{
	disconnect();
}


Device_component::~Device_component()
{
	_interfaces.for_each([&] (Interface_component & ic) {
		destroy(_heap, &ic); });
}


template <typename FN>
void Session_component::_device_policy(genode_usb_device const &d,
                                       FN const                &fn)
{
	using Label = genode_usb_device::Label;

	try {
		Session_policy const policy(label(), _config.xml());

		policy.for_each_sub_node("device", [&] (Xml_node & node) {
			uint16_t vendor  = node.attribute_value<uint16_t>("vendor_id", 0);
			uint16_t product = node.attribute_value<uint16_t>("product_id", 0);
			Label    label   = node.attribute_value("name", Label());

			bool match = (((vendor == d.desc.vendor_id) &&
			              (product == d.desc.product_id)) ||
			             (d.label() == label));

			enum { CLASS_AUDIO = 0x1, CLASS_HID = 0x3 };
			if (!match) {
				uint8_t cla = node.attribute_value<uint8_t>("class", 0);
				bool found_audio = false;
				d.configs.for_each([&] (genode_usb_configuration const &c) {
					if (!c.active)
						return;
					c.interfaces.for_each([&] (genode_usb_interface const &i) {
						if (i.desc.iclass == cla) match = true;
						if (i.desc.iclass == CLASS_AUDIO) found_audio = true;
					});
				});
				/* do not match HID in AUDIO devices */
				if (match && cla == CLASS_HID && found_audio)
					match = false;
			}

			if (match) fn(node);
		});
	} catch(Session_policy::No_policy_defined) {}
}


bool Session_component::_matches(genode_usb_device const & d)
{
	bool ret = false;
	_device_policy(d, [&] (Xml_node) { ret = true; });
	return ret;
}


Device_capability
Session_component::_acquire(genode_usb_device::Label const &name, bool controls)
{
	Device_component * dc = new (_heap)
		Device_component(_env, _heap, _device_sessions, *this,
		                 controls, name, _sigh_cap);
	return dc->session_cap();
}


void Session_component::_release(Device_component &dc)
{
	genode_usb_device::Label name = dc._device_label;
	destroy(_heap, &dc);

	_devices.for_each([&] (genode_usb_device & device) {
		if (device.label() != name)
			return;
		_sessions.for_each([&] (Session_component &sc) {
			if (sc._matches(device)) sc.update_devices_rom(); });
		_root.report();
	});
}


void Session_component::set_interface(genode_usb_device::Label label,
                                      uint16_t num, uint16_t alt)
{
	bool changed = false;
	_devices.for_each([&] (genode_usb_device & d) {
		if (d.label() != label)
			return;
		d.configs.for_each([&] (genode_usb_configuration & c) {
			if (!c.active)
				return;
			c.interfaces.for_each([&] (genode_usb_interface & i) {
				if (i.desc.number != num)
					return;

				if (i.active != (i.desc.alt_settings == alt)) {
					i.active = (i.desc.alt_settings == alt);
					changed = true;
				}
			});
		});
	});

	if (changed) {
		update_devices_rom();
		_root.report();
	}
}


void Session_component::set_configuration(genode_usb_device::Label label,
                                          uint16_t num)
{
	bool changed = false;
	_devices.for_each([&] (genode_usb_device & d) {
		if (d.label() != label)
			return;
		d.configs.for_each([&] (genode_usb_configuration & c) {
			if (c.active != (c.desc.config_value == num)) {
				c.active = (c.desc.config_value == num);
				changed = true;
			}
		});
	});

	if (changed) {
		update_devices_rom();
		_root.report();
	}
}


bool Session_component::matches(genode_usb_device::Label label, uint8_t iface)
{
	/*
	 * If no interface is defined in the session policy,
	 * all interfaces are allowed, otherwise check for the iface number
	 */
	bool ret = false;
	_devices.for_each([&] (genode_usb_device const & d) {
		if (d.label() != label)
			return;
		_device_policy(d, [&] (Xml_node dev_node) {
			if (!dev_node.has_sub_node("interface"))
				ret = true;
			else
				dev_node.for_each_sub_node("interface", [&] (Xml_node & node) {
					if (node.attribute_value<uint8_t>("number", 255) == iface)
						ret = true;
				});
		});
	});
	return ret;
}


template <typename FN>
void Session_component::for_each_ep(genode_usb_device::Label label,
                                    uint8_t iface_idx, FN const & fn)
{
	_devices.for_each([&] (genode_usb_device const & d) {
		if (d.label() != label)
			return;
		d.configs.for_each([&] (genode_usb_configuration const & cfg) {
			if (!cfg.active)
				return;
			cfg.interfaces.for_each([&] (genode_usb_interface const & iface) {
				if (iface.desc.number == iface_idx)
					iface.endpoints.for_each(fn);
			});
		});
	});
}


void Session_component::announce_device(genode_usb_device const & device)
{
	if (_matches(device)) update_devices_rom();
}


void Session_component::discontinue_device(genode_usb_device const & device)
{
	_device_sessions.for_each([&] (Device_component & dc) {
		if (dc._device_label != device.label())
			return;
		dc.disconnect();
		update_devices_rom();
	});
}


void Session_component::update_policy()
{
	_device_sessions.for_each([&] (Device_component & dc) {
		_devices.for_each([&] (genode_usb_device const & device) {
			if (device.label() != dc._device_label)
				return;
			if (!_matches(device)) {
				dc.disconnect();
				_release_fn(device.bus, device.dev);
			}
		});
	});
	update_devices_rom();
}


void Session_component::update_devices_rom()
{
	_rom_session.trigger_update();
}


Rom_session_capability Session_component::devices_rom()
{
	return _rom_session.cap();
}


bool Session_component::acquired(genode_usb_device const &dev)
{
	if (_state == IN_DESTRUCTION)
		return false;

	bool ret = false;
	_device_sessions.for_each([&] (Device_component & dc) {
		if (dc._device_label == dev.label()) ret = dc.connected(); });
	return ret;
}


bool Session_component::request(genode_usb_device         const &dev,
                                genode_usb_req_callback_t const  callback,
                                void                            *opaque_data)
{
	bool ret = false;
	_device_sessions.for_each([&] (Device_component & dc) {
		if (dc._device_label == dev.label())
			if (dc.request(callback, opaque_data)) ret = true;
	});
	return ret;
}


bool
Session_component::handle_response(genode_usb_request_handle_t handle,
                                   genode_usb_request_ret_t    v,
                                   uint32_t                   *actual_sizes)
{
	bool handled = false;
	_device_sessions.for_each([&] (Device_component & dc) {
		if (!handled) handled = dc.handle_response(handle, v, actual_sizes); });
	return handled;
}


void Session_component::handle_disconnected()
{
	_device_sessions.for_each([&] (Device_component & dc) {
		dc.handle_disconnected(); });
}


Device_capability Session_component::acquire_device(Device_name const &name)
{
	Device_capability cap;
	bool found = false;

	_devices.for_each([&] (genode_usb_device & device) {
		if (device.label() != name || !_matches(device))
			return;

		found = true;
		_sessions.for_each([&] (Session_component &sc) {
			if (sc.acquired(device)) found = false; });

		if (!found) {
			warning("USB device ", name,
			        "already acquired by another session");
		}

		cap = _acquire(device.label(), true);
		_sessions.for_each([&] (Session_component &sc) {
			if (sc._matches(device)) sc.update_devices_rom(); });
		_root.report();
	});

	if (!found)
		cap = (new (_heap)
			Device_component(_env, _heap, _device_sessions, *this,
			                 false, _sigh_cap))->session_cap();
	return cap;
}


Device_capability Session_component::acquire_single_device()
{
	Device_capability cap;
	_devices.for_each([&] (genode_usb_device & device) {
		if (cap.valid() || !_matches(device))
			return;

		bool acquired = false;
		_sessions.for_each([&] (Session_component &sc) {
			if (sc.acquired(device)) acquired = true; });

		if (acquired)
			return;

		cap = _acquire(device.label(), true);
		_sessions.for_each([&] (Session_component &sc) {
			if (sc._matches(device)) sc.update_devices_rom(); });
		_root.report();
	});
	return cap;
}


void Session_component::release_device(Device_capability cap)
{
	if (!cap.valid())
		return;

	_device_sessions.for_each([&] (Device_component & dc) {
		if (cap.local_name() == dc.cap().local_name())
			_release(dc); });
}


void Session_component::produce_xml(Xml_generator &xml)
{
	_devices.for_each([&] (genode_usb_device const & device) {
		if (!_matches(device))
			return;

		bool acquired_by_other_session = false;
		_sessions.for_each([&] (Session_component &sc) {
			if (sc.acquired(device) && &sc != this)
				acquired_by_other_session = true;
		});
		if (acquired_by_other_session)
			return;

		device.generate(xml, acquired(device));
	});
}


void Session_component::wakeup()
{
	_device_sessions.for_each([&] (Device_component & dc) { dc.wakeup(); });
}


Session_component::Session_component(Env                                   &env,
                                     ::Root                                &root,
                                     Reg_list<Session_component>           &registry,
                                     Reg_list<genode_usb_device>           &devices,
                                     Attached_rom_dataspace                &config,
                                     Signal_context_capability              sigh_cap,
                                     genode_shared_dataspace_alloc_attach_t alloc_fn,
                                     genode_shared_dataspace_free_t         free_fn,
                                     genode_usb_dev_release_t               release_rn,
                                     Label     const                       &label,
                                     Resources const                       &resources,
                                     Diag      const                       &diag)
:
	Session_object<Usb::Session>(env.ep(), resources, label, diag),
	Reg_list<Session_component>::Element(registry, *this),
	Dynamic_rom_session::Xml_producer("devices"),
	_env(env),
	_root(root),
	_sessions(registry),
	_devices(devices),
	_config(config),
	_sigh_cap(sigh_cap),
	_alloc_fn(alloc_fn),
	_free_fn(free_fn),
	_release_fn(release_rn) { }


Session_component::~Session_component()
{
	_state = IN_DESTRUCTION;
	_device_sessions.for_each([&] (Device_component & dc) {
		_devices.for_each([&] (genode_usb_device & device) {
			if (device.label() == dc._device_label)
				_release_fn(device.bus, device.dev); });

		_release(dc);
	});
}


Session_component * ::Root::_create_session(const char * args,
                                             Affinity const &)
{
	Session_component * sc = nullptr;
	try {
		Session_label  const label  { session_label_from_args(args) };
		Session_policy const policy { label, _config.xml()          };

		sc = new (md_alloc())
			Session_component(_env, *this, _sessions, _devices, _config,
			                  _sigh_cap, _alloc_fn, _free_fn, _release_fn,
			                  label, session_resources_from_args(args),
			                  session_diag_from_args(args));
	} catch (Session_policy::No_policy_defined) {
		error("Invalid session request, no matching policy for ",
		      "'", label_from_args(args).string(), "'");
		throw Service_denied();
	} catch (...) {
		if (sc) { Genode::destroy(md_alloc(), sc); }
		throw;
	}
	return sc;
}


void ::Root::_upgrade_session(Session_component * sc, const char * args)
{
	sc->upgrade(ram_quota_from_args(args));
	sc->upgrade(cap_quota_from_args(args));
}


void ::Root::report()
{
	if (!_device_reporter.constructed())
		return;

	_device_reporter->generate([&] (Reporter::Xml_generator &xml) {
		_devices.for_each([&] (genode_usb_device & d) {
			bool acquired = false;
			_sessions.for_each([&] (Session_component &sc) {
				if (sc.acquired(d)) acquired = true; });
			d.generate(xml, acquired);
		});
	});
}


void ::Root::_config_update()
{
	/*
	 * Defer the startup of the USB driver until the first configuration
	 * becomes available. This is needed in scenarios where the configuration
	 * is dynamically generated and supplied to the USB driver via the
	 * report-ROM service.
	 */
	_config.update();

	/*
	 * Check for report policy, and resp. con-/destruct device reporter
	 */
	_config.xml().with_optional_sub_node("report", [&] (Xml_node node) {
		_device_reporter.conditional(node.attribute_value("devices", false),
		                             _env, "devices", "devices" );

		_config_reporter.enabled(node.attribute_value("config", false));
	});

	/*
	 * Report the own configuration to show management component
	 * that we've consumed the configuration
	 */
	Reporter::Xml_generator xml(_config_reporter, [&] {
		xml.attribute("bios_handoff",
		              _config.xml().attribute_value("bios_handoff", true));
		_config.xml().with_raw_content([&] (char const *start, size_t len) {
			xml.append(start, len); });
	});

	_announce_service();

	_sessions.for_each([&] (Session_component & sc) {
		sc.update_policy(); });
}


void ::Root::_announce_service()
{
	if (_announced)
		return;

	if (_config.xml().type() == "config") {
		_env.parent().announce(_env.ep().manage(*this));
		_announced = true;
	}
}


void ::Root::device_add_endpoint(struct genode_usb_interface   *iface,
                                 genode_usb_endpoint_descriptor desc)
{
	new (_heap) genode_usb_endpoint(iface->endpoints, desc);
}


void ::Root::device_add_interface(struct genode_usb_configuration *cfg,
                                  genode_usb_dev_string_item_t     info_string,
                                  genode_usb_interface_descriptor  desc,
                                  genode_usb_dev_add_endp_t        callback,
                                  void                            *opaque_data,
                                  bool                             active)
{
	String_item info { string_item(info_string, opaque_data) };

	genode_usb_interface *iface = new (_heap)
		genode_usb_interface(cfg->interfaces, info, desc, active);
	for (unsigned i = desc.num_endpoints; i > 0; i--)
		callback(iface, i-1, opaque_data);
}


void ::Root::device_add_configuration(struct genode_usb_device    *dev,
                                      genode_usb_config_descriptor desc,
                                      genode_usb_dev_add_iface_t   callback,
                                      void                        *opaque_data,
                                      bool                         active)
{
	genode_usb_configuration *config = new (_heap)
		genode_usb_configuration(dev->configs, desc, active);
	for (unsigned i = desc.num_interfaces; i > 0; i--)
		callback(config, i-1, opaque_data);
}


void ::Root::announce_device(genode_usb_bus_num_t         bus,
                             genode_usb_dev_num_t         dev,
                             genode_usb_speed_t           speed,
                             genode_usb_dev_string_item_t manufacturer_string,
                             genode_usb_dev_string_item_t product_string,
                             genode_usb_device_descriptor desc,
                             genode_usb_dev_add_config_t  callback,
                             void                        *opaque_data)
{
	String_item manufacturer { string_item(manufacturer_string, opaque_data) };
	String_item product      { string_item(product_string, opaque_data) };

	genode_usb_device *device = new (_heap)
		genode_usb_device(_devices, bus, dev, speed, manufacturer, product, desc);
	for (unsigned i = desc.num_configs; i > 0; i--)
		callback(device, i-1, opaque_data);
	_announce_service();
	report();
	_sessions.for_each([&] (Session_component & sc) {
		sc.announce_device(*device); });
}


void ::Root::discontinue_device(genode_usb_bus_num_t bus,
                                genode_usb_dev_num_t dev)
{
	_devices.for_each([&] (genode_usb_device & device)
	{
		if (device.bus != bus || device.dev != dev)
			return;

		_sessions.for_each([&] (Session_component & sc) {
			sc.discontinue_device(device); });

		device.configs.for_each([&] (genode_usb_configuration & cfg) {
			cfg.interfaces.for_each([&] (genode_usb_interface & iface) {
				iface.endpoints.for_each([&] (genode_usb_endpoint & endp) {
					Genode::destroy(_heap, &endp); });
				Genode::destroy(_heap, &iface);
			});
			Genode::destroy(_heap, &cfg);
		});
		Genode::destroy(_heap, &device);
	});
	report();
}


bool ::Root::acquired(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev)
{
	bool ret = false;
	_devices.for_each([&] (genode_usb_device & device) {
		if (device.bus == bus && device.dev == dev)
			_sessions.for_each([&] (Session_component & sc) {
				if (sc.acquired(device)) ret = true; });
	});
	return ret;
}


bool ::Root::request(genode_usb_bus_num_t            bus,
                     genode_usb_dev_num_t            dev,
                     genode_usb_req_callback_t const callback,
                     void                           *opaque_data)
{
	bool ret = false;
	_devices.for_each([&] (genode_usb_device & device)
	{
		if (device.bus != bus || device.dev != dev)
			return;

		_sessions.for_each([&] (Session_component & sc) {
			if (sc.request(device, callback, opaque_data)) ret = true; });
	});
	return ret;
}


void ::Root::handle_response(genode_usb_request_handle_t id,
                             genode_usb_request_ret_t    ret,
                             uint32_t                   *actual_sizes)
{
	bool handled = false;
	_sessions.for_each([&] (Session_component & sc) {
		if (!handled) handled = sc.handle_response(id, ret, actual_sizes);
	});
}


void ::Root::handle_disconnected_sessions()
{
	_sessions.for_each([&] (Session_component & sc) {
		sc.handle_disconnected(); });
}


void ::Root::wakeup()
{
	_sessions.for_each([&] (Session_component & sc) { sc.wakeup(); });
}


::Root::Root(Env                                   &env,
             Signal_context_capability              cap,
             genode_shared_dataspace_alloc_attach_t alloc_fn,
             genode_shared_dataspace_free_t         free_fn,
             genode_usb_dev_release_t               release_fn)
:
	Sliced_heap(env.ram(), env.rm()),
	Root_component<Session_component>(env.ep(), *this),
	_env(env), _sigh_cap(cap), _alloc_fn(alloc_fn),
	_free_fn(free_fn), _release_fn(release_fn)
{
	_config.sigh(_config_handler);
	_config_update();
}


void
Genode_c_api::initialize_usb_service(Env                                   &env,
                                     Signal_context_capability              sigh,
                                     genode_shared_dataspace_alloc_attach_t alloc_fn,
                                     genode_shared_dataspace_free_t         free_fn,
                                     genode_usb_dev_release_t               release_fn)
{
	static ::Root root(env, sigh, alloc_fn, free_fn, release_fn);
	_usb_root = &root;
}


extern "C"
void genode_usb_device_add_endpoint(struct genode_usb_interface   *iface,
                                    genode_usb_endpoint_descriptor desc)
{
	if (_usb_root) _usb_root->device_add_endpoint(iface, desc);
}


extern "C"
void genode_usb_device_add_interface(struct genode_usb_configuration *cfg,
                                     genode_usb_dev_string_item_t     info_string,
                                     genode_usb_interface_descriptor  desc,
                                     genode_usb_dev_add_endp_t        callback,
                                     void                            *opaque_data,
                                     bool                             active)
{
	if (_usb_root)
		_usb_root->device_add_interface(cfg, info_string, desc, callback,
		                                opaque_data, active);
}


extern "C"
void genode_usb_device_add_configuration(struct genode_usb_device    *dev,
                                         genode_usb_config_descriptor desc,
                                         genode_usb_dev_add_iface_t   callback,
                                         void                        *opaque_data,
                                         bool                         active)
{
	if (_usb_root)
		_usb_root->device_add_configuration(dev, desc, callback,
		                                    opaque_data, active);
}



extern "C"
void genode_usb_announce_device(genode_usb_bus_num_t         bus,
                                genode_usb_dev_num_t         dev,
                                genode_usb_speed_t           speed,
                                genode_usb_dev_string_item_t manufacturer_string,
                                genode_usb_dev_string_item_t product_string,
                                genode_usb_device_descriptor desc,
                                genode_usb_dev_add_config_t  callback,
                                void                        *opaque_data)
{
	if (_usb_root)
		_usb_root->announce_device(bus, dev, speed,
		                           manufacturer_string, product_string,
		                           desc, callback, opaque_data);
}


extern "C" void genode_usb_discontinue_device(genode_usb_bus_num_t bus,
                                              genode_usb_dev_num_t dev)
{
	if (_usb_root) _usb_root->discontinue_device(bus, dev);
}


extern "C" bool
genode_usb_device_acquired(genode_usb_bus_num_t bus,
                           genode_usb_dev_num_t dev)
{
	return _usb_root ? _usb_root->acquired(bus, dev) : false;
}


extern "C" bool
genode_usb_request_by_bus_dev(genode_usb_bus_num_t            bus,
                              genode_usb_dev_num_t            dev,
                              genode_usb_req_callback_t const callback,
                              void                           *opaque_data)
{
	return _usb_root ? _usb_root->request(bus, dev, callback, opaque_data)
	                 : false;
}


extern "C" void
genode_usb_ack_request(genode_usb_request_handle_t request_id,
                       genode_usb_request_ret_t    ret,
                       uint32_t                   *actual_sizes)
{
	if (_usb_root)
		_usb_root->handle_response(request_id, ret, actual_sizes);
}


extern "C" void genode_usb_notify_peers()
{
	if (_usb_root) _usb_root->wakeup();
}


extern "C" void genode_usb_handle_disconnected_sessions()
{
	if (_usb_root) _usb_root->handle_disconnected_sessions();
}
