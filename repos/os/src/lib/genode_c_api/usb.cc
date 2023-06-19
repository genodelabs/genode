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
#include <root/component.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <usb_session/rpc_object.h>
#include <util/bit_allocator.h>

#include <genode_c_api/usb.h>

using namespace Genode;

struct Device {
	genode_usb_vendor_id_t  vendor;
	genode_usb_product_id_t product;
	genode_usb_class_num_t  cla;
	genode_usb_bus_num_t    bus;
	genode_usb_dev_num_t    dev;
	genode_usb_session    * usb_session { nullptr };

	Device(genode_usb_vendor_id_t  vendor,
	       genode_usb_product_id_t product,
	       genode_usb_class_num_t  cla,
	       genode_usb_bus_num_t    bus,
	       genode_usb_dev_num_t    dev)
	: vendor(vendor), product(product),
	  cla(cla), bus(bus), dev(dev) {}

	using Label = String<64>;

	Label label() { return Label("usb-", bus, "-", dev); }
};


class Session_id
{
	private:

		genode_usb_session_handle_t const _id;
		unsigned                          _ref_cnt { 0 };

	public:

		Session_id(genode_usb_session_handle_t id) : _id(id) {}

		genode_usb_session_handle_t id() { return _id; }

		void inc()  { _ref_cnt++; }
		void dec()  { _ref_cnt--; }
		bool used() { return _ref_cnt; }
};

class Root;


class genode_usb_session : public Usb::Session_rpc_object
{
	private:

		friend class ::Root;

		enum { MAX_PACKETS_IN_FLIGHT = 32 };

		Constructible<Usb::Packet_descriptor> packets[MAX_PACKETS_IN_FLIGHT] { };

		::Root                         & _root;
		genode_shared_dataspace        * _ds;
		Signal_context_capability        _sigh_state_cap {};
		Session_id                     & _id;
		Session_label const              _label;
		List_element<genode_usb_session> _le { this };

		unsigned _packets_in_flight();
		void     _ack(int err, Usb::Packet_descriptor p);

		genode_usb_buffer _buffer_of_packet(Usb::Packet_descriptor p);

		/*
		 * Non_copyable
		 */
		genode_usb_session(const genode_usb_session&);
		genode_usb_session & operator=(const genode_usb_session&);

	public:

		genode_usb_session(::Root                    & root,
		                   genode_shared_dataspace   * ds,
		                   Env                       & env,
		                   Session_id                & id,
		                   Signal_context_capability   cap,
		                   Session_label               label);

		virtual ~genode_usb_session() {}

		/*
		 * Handle one outstanding request from the packet-stream
		 * of this session
		 *
		 * \param callbacks  C-API callback struct to handle request
		 * \param data       opaque pointer for C-side to identify related data
		 * \returns true if a request got handled, false if nothing was done
		 */
		bool request(genode_usb_request_callbacks & callbacks, void * data);

		/*
		 * Handle response for an asynchronous request
		 *
		 * \param handle    handle that identifies the original request
		 * \param callback  C-API callback to handle the response
		 * \param data       opaque pointer for C-side to identify related data
		 */
		void handle_response(genode_usb_request_handle_t handle,
		                     genode_usb_response_t       callback,
		                     void *                      data);

		/*
		 * Notify session about state changes
		 */
		void notify();

		/*
		 * Acknowledge all remaining requests in the packet stream
		 */
		void flush_packet_stream();


		/***************************
		 ** USB session interface **
		 ***************************/

		void sigh_state_change(Signal_context_capability sigh) override;
		bool plugged() override;
		void config_descriptor(Usb::Device_descriptor *device_descr,
		                       Usb::Config_descriptor *config_descr) override;
		unsigned alt_settings(unsigned index) override;
		void interface_descriptor(unsigned index, unsigned alt_setting,
		                          Usb::Interface_descriptor *interface_descr) override;
		bool interface_extra(unsigned index, unsigned alt_setting,
		                     Usb::Interface_extra *interface_data) override;
		void endpoint_descriptor(unsigned              interface_num,
		                         unsigned              alt_setting,
		                         unsigned              endpoint_num,
		                         Usb::Endpoint_descriptor  *endpoint_descr) override;
		void claim_interface(unsigned interface_num) override;
		void release_interface(unsigned interface_num) override;
};


class Root : public Root_component<genode_usb_session>
{
	private:

		enum { MAX_DEVICES = 32, MAX_SESSIONS = MAX_DEVICES };

		Env                     & _env;
		Signal_context_capability _sigh_cap;
		Attached_rom_dataspace    _config   { _env, "config"  };
		Signal_handler<Root>      _config_handler { _env.ep(), *this,
		                                            &Root::_config_update };
		Reporter                  _config_reporter { _env, "config"  };
		Constructible<Session_id> _session_ids[MAX_SESSIONS];
		Constructible<Device>     _devices[MAX_DEVICES];
		bool                      _announced { false };

		Constructible<Expanding_reporter>      _device_reporter {};
		List<List_element<genode_usb_session>> _sessions {};

		Root(const Root&);
		Root & operator=(const Root&);

		genode_usb_session * _create_session(const char * args,
		                                     Affinity const &) override;

		void _destroy_session(genode_usb_session * session) override;

		template <typename FUNC>
		void _for_each_device(FUNC const & fn)
		{
			for (auto &device : _devices)
				if (device.constructed())
					fn(*device);
		}

		template <typename FUNC>
		void _for_each_session(FUNC const & fn)
		{
			for (List_element<genode_usb_session> * s = _sessions.first();
			     s; s = s->next())
				fn(*s->object());
		}

		/*
		 * Update report about existing USB devices
		 */
		void _report();

		/*
		 * Returns true if given device matches the session's policy
		 */
		bool _matches(Device & d, genode_usb_session & s);

		void _config_update();
		void _announce_service();

	public:

		Root(Env & env, Allocator & alloc, Signal_context_capability);

		void announce_device(genode_usb_vendor_id_t  vendor,
		                     genode_usb_product_id_t product,
		                     genode_usb_class_num_t  cla,
		                     genode_usb_bus_num_t    bus,
		                     genode_usb_dev_num_t    dev);

		void discontinue_device(genode_usb_bus_num_t bus,
		                        genode_usb_dev_num_t dev);

		/*
		 * Returns the session's handle for a given device,
		 * or an invalid handle if there is no active session for the device
		 */
		genode_usb_session_handle_t session(genode_usb_bus_num_t bus,
		                                    genode_usb_dev_num_t dev);

		/*
		 * Finds the bus/device numbers of the device associated for a
		 * given session
		 *
		 * \returns true if a device is associated, otherwise false
		 */
		bool device_associated(genode_usb_session   * session,
		                       genode_usb_bus_num_t & bus,
		                       genode_usb_dev_num_t & dev);

		/*
		 * Apply a functor to the session with the given handle
		 */
		template <typename FUNC>
		void session(genode_usb_session_handle_t id, FUNC const & fn);

		/*
		 * Acknowledge requests from sessions without device
		 */
		void handle_empty_sessions();

		void decrement_session_id(genode_usb_session_handle_t id);
};


static ::Root                   * _usb_root  = nullptr;
static genode_usb_rpc_callbacks * _callbacks = nullptr;


void genode_usb_session::sigh_state_change(Signal_context_capability sigh)
{
	_sigh_state_cap = sigh;
}


void genode_usb_session::notify()
{
	Signal_transmitter state_changed { _sigh_state_cap };
	state_changed.submit();
}


void genode_usb_session::flush_packet_stream()
{
	/* ack packets in flight */
	for (auto &p : packets) {
		if (!p.constructed())
			continue;
		_ack(Usb::Packet_descriptor::NO_DEVICE_ERROR, *p);
		p.destruct();
	}

	/* ack all packets in request stream */
	while (sink()->packet_avail() && sink()->ack_slots_free())
		_ack(Usb::Packet_descriptor::NO_DEVICE_ERROR, sink()->get_packet());
}


bool genode_usb_session::plugged()
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	return _root.device_associated(this, bus, dev);
}


void genode_usb_session::config_descriptor(Usb::Device_descriptor * device_descr,
                                           Usb::Config_descriptor * config_descr)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	unsigned speed = _callbacks->cfg_desc_fn(bus, dev, (void*) device_descr,
	                                         config_descr);
	device_descr->num   = dev;
	device_descr->speed = speed;
}


unsigned genode_usb_session::alt_settings(unsigned index)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	int err = _callbacks->alt_settings_fn(bus, dev, index);
	if (err < 0)
		throw Interface_not_found();
	return (unsigned) err;
}


void genode_usb_session::interface_descriptor(unsigned index,
                                              unsigned alt_setting,
                                              Usb::Interface_descriptor * desc)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	int active;
	if (_callbacks->iface_desc_fn(bus, dev, index, alt_setting, (void*)desc,
	                              sizeof(Usb::Interface_descriptor), &active))
		throw Interface_not_found();

	if (active)
		desc->active = true;
}


bool genode_usb_session::interface_extra(unsigned index,
                                         unsigned alt_setting,
                                         Usb::Interface_extra * interface_data)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	int len =
		_callbacks->iface_extra_fn(bus, dev, index, alt_setting,
	                               (void*)interface_data->data,
	                               sizeof(interface_data->data));
	if (len < 0)
		throw Interface_not_found();

	if (len >= 0xff)
		error("Unsupported length of alt_setting iface extra!");

	interface_data->length = (uint8_t)len;
	return len;
}


void genode_usb_session::endpoint_descriptor(unsigned interface_num,
                                             unsigned alt_setting,
                                             unsigned endpoint_num,
                                             Usb::Endpoint_descriptor * endp)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	if (_callbacks->endp_desc_fn(bus, dev, interface_num, alt_setting,
	                             endpoint_num, (void*)endp,
	                             sizeof(Usb::Endpoint_descriptor)))
		throw Interface_not_found();
}


void genode_usb_session::claim_interface(unsigned iface)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	if (_callbacks->claim_fn(bus, dev, iface))
		throw Interface_not_found();
}


void genode_usb_session::release_interface(unsigned iface)
{
	genode_usb_bus_num_t bus;
	genode_usb_dev_num_t dev;
	if (!_root.device_associated(this, bus, dev))
		throw Device_not_found();

	if (_callbacks->release_fn(bus, dev, iface))
		throw Interface_not_found();
}


unsigned genode_usb_session::_packets_in_flight()
{
	unsigned ret = 0;
	for (auto const &p : packets)
		if (p.constructed()) ret++;
	return ret;
}


void genode_usb_session::_ack(int err, Usb::Packet_descriptor p)
{
	if (err == NO_ERROR)
		p.succeded = true;
	else
		p.error = (Usb::Packet_descriptor::Error)err;
	try {
		sink()->acknowledge_packet(p);
	} catch(...) {
		warning("USB client's ack queue run full, looses packet ack!");
	}
}


genode_usb_buffer genode_usb_session::_buffer_of_packet(Usb::Packet_descriptor p)
{
	void * addr = nullptr;
	void * packet_content = sink()->packet_content(p);
	if (packet_content) {
		addr_t offset = (addr_t)packet_content - (addr_t)sink()->ds_local_base();

		addr = (void*)(genode_shared_dataspace_local_address(_ds) + offset);
	}

	return { addr, addr ? p.size() : 0 };
}


bool genode_usb_session::request(genode_usb_request_callbacks & req, void * data)
{
	using Packet_descriptor = Usb::Packet_descriptor;

	genode_usb_request_handle_t idx = 0;

	/* find free packet slot */
	for (auto const &p : packets) {
		if (!p.constructed())
			break;
		++idx;
	}
	if (idx == MAX_PACKETS_IN_FLIGHT)
		return false;

	Packet_descriptor p;

	/* get next packet from request stream */
	while (true) {
		if (!sink()->packet_avail() ||
		    (sink()->ack_slots_free() <= _packets_in_flight()))
			return false;

		p = sink()->get_packet();
		if (sink()->packet_valid(p))
			break;

		_ack(Packet_descriptor::PACKET_INVALID_ERROR, p);
	}

	genode_usb_buffer payload = _buffer_of_packet(p);

	packets[idx].construct(p);
	_id.inc(); /* increment the session ids usage */

	switch (p.type) {
	case Packet_descriptor::STRING:
		req.string_fn((genode_usb_request_string*)&p.string,
		              _id.id(), idx, payload, data);
		break;
	case Packet_descriptor::CTRL:
		req.urb_fn({ CTRL, &p.control }, _id.id(), idx, payload, data);
		break;
	case Packet_descriptor::BULK:
		req.urb_fn({ BULK, &p.transfer }, _id.id(), idx, payload, data);
		break;
	case Packet_descriptor::IRQ:
		req.urb_fn({ IRQ, &p.transfer }, _id.id(), idx, payload, data);
		break;
	case Packet_descriptor::ISOC:
		req.urb_fn({ ISOC, &p.transfer }, _id.id(), idx, payload, data);
		break;
	case Packet_descriptor::ALT_SETTING:
		req.altsetting_fn(p.interface.number, p.interface.alt_setting,
		                  _id.id(), idx, data);
		break;
	case Packet_descriptor::CONFIG:
		req.config_fn(p.number, _id.id(), idx, data);
		break;
	case Packet_descriptor::RELEASE_IF:
		warning("Release interface gets ignored!");
		_id.dec();
		packets[idx].destruct();
		break;
	case Packet_descriptor::FLUSH_TRANSFERS:
		req.flush_fn(p.number, _id.id(), idx, data);
		break;
	};

	return true;
}


void genode_usb_session::handle_response(genode_usb_request_handle_t id,
                                         genode_usb_response_t       callback,
                                         void                      * callback_data)
{
	using Packet_descriptor = Usb::Packet_descriptor;

	Packet_descriptor p       = *packets[id];
	genode_usb_buffer payload = _buffer_of_packet(p);
	switch (p.type) {
	case Packet_descriptor::CTRL:
		_ack(callback({ CTRL, &p.control }, payload, callback_data), p);
		break;
	case Packet_descriptor::BULK:
		_ack(callback({ BULK, &p.transfer }, payload, callback_data), p);
		break;
	case Packet_descriptor::IRQ:
		_ack(callback({ IRQ, &p.transfer }, payload, callback_data), p);
		break;
	case Packet_descriptor::ISOC:
		_ack(callback({ ISOC, &p.transfer }, payload, callback_data), p);
		break;
	default:
		_ack(callback({ NONE, nullptr }, { nullptr, 0 }, callback_data), p);
	};
	packets[id].destruct();
}


genode_usb_session::genode_usb_session(::Root                    & root,
                                       genode_shared_dataspace   * ds,
                                       Env                       & env,
                                       Session_id                & id,
                                       Signal_context_capability   cap,
                                       Session_label const         label)
:
	Usb::Session_rpc_object(genode_shared_dataspace_capability(ds),
	                        env.ep().rpc_ep(), env.rm()),
	_root(root),
	_ds(ds),
	_id(id),
	_label(label)
{
	_tx.sigh_packet_avail(cap);

	id.inc();
}


bool ::Root::_matches(Device & d, genode_usb_session & s)
{
	try {
		Session_policy const policy(s._label, _config.xml());

		genode_usb_vendor_id_t  vendor  =
			policy.attribute_value<genode_usb_vendor_id_t>("vendor_id", 0);
		genode_usb_product_id_t product =
			policy.attribute_value<genode_usb_product_id_t>("product_id", 0);
		genode_usb_bus_num_t    bus     =
			policy.attribute_value<genode_usb_bus_num_t>("bus", 0);
		genode_usb_dev_num_t    dev     =
			policy.attribute_value<genode_usb_dev_num_t>("dev", 0);
		genode_usb_class_num_t  cla     =
			policy.attribute_value<genode_usb_class_num_t>("class", 0);

		if (bus && dev)
			return (bus == d.bus) && (dev == d.dev);
		if (vendor && product)
			return (vendor == d.vendor) && (product == d.product);
		if (cla)
			return (cla == d.cla) && (d.label() == s._label.last_element());
	} catch(Session_policy::No_policy_defined) {}

	return false;
}


genode_usb_session * ::Root::_create_session(const char * args,
                                             Affinity const &)
{
	Session_label const label = label_from_args(args);

	size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
	size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

	if (!tx_buf_size)
		throw Service_denied();

	if (tx_buf_size > ram_quota) {
		warning("Insufficient RAM quota, got ", ram_quota, " need ",
		        tx_buf_size);
		throw Insufficient_ram_quota();
	}

	unsigned i = 0;
	for (; i < MAX_SESSIONS; i++)
		if (!_session_ids[i]->used())
			break;
	if (i >= MAX_SESSIONS) {
		warning("Maximum of sessions reached!");
		throw Service_denied();
	}

	genode_shared_dataspace * ds  = _callbacks->alloc_fn(tx_buf_size);
	genode_usb_session      * ret = new (md_alloc())
		genode_usb_session(*this, ds, _env, *_session_ids[i], _sigh_cap, label);
	_sessions.insert(&ret->_le);

	if (!ret) throw Service_denied();

	bool found_device = false;
	_for_each_device([&] (Device & d) {
		if (found_device || d.usb_session || !_matches(d, *ret))
			return;
		d.usb_session = ret;
		found_device = true;
	});

	return ret;
}


void ::Root::_destroy_session(genode_usb_session * session)
{
	_for_each_device([&] (Device & d) {
		if (d.usb_session == session) {
			_callbacks->release_all_fn(d.bus, d.dev);
			d.usb_session = nullptr;
		}
	});

	session->_id.dec();
	genode_shared_dataspace   * ds = session->_ds;
	_sessions.remove(&session->_le);
	Genode::destroy(md_alloc(), session);
	_callbacks->free_fn(ds);
}


void ::Root::_report()
{
	using Value = String<64>;

	if (!_device_reporter.constructed())
		return;

	_device_reporter->generate([&] (Reporter::Xml_generator &xml) {
		_for_each_device([&] (Device & d) {
			xml.node("device", [&] {
				xml.attribute("label",      d.label());
				xml.attribute("vendor_id",  Value(Hex(d.vendor)));
				xml.attribute("product_id", Value(Hex(d.product)));
				xml.attribute("bus",        Value(Hex(d.bus)));
				xml.attribute("dev",        Value(Hex(d.dev)));
				xml.attribute("class",      Value(Hex(d.cla)));

				Usb::Device_descriptor device_descr { };
				Usb::Config_descriptor config_descr { };

				if (!_callbacks->cfg_desc_fn(d.bus, d.dev,
				                             (void*) &device_descr,
				                             &config_descr))
					return;

				for (unsigned idx = 0; idx < config_descr.num_interfaces; idx++) {

					int const num_altsetting = _callbacks->alt_settings_fn(d.bus, d.dev, idx);
					if (num_altsetting < 0)
						continue;

					for (int adx = 0; adx < num_altsetting; adx++) {

						int active = false;
						Usb::Interface_descriptor iface_desc { };
						if (_callbacks->iface_desc_fn(d.bus, d.dev, idx, adx,
						                              (void*) &iface_desc,
						                              sizeof (Usb::Interface_descriptor),
						                              &active))
							continue;

						if (!active)
							continue;

						xml.node("interface", [&] {
							xml.attribute("class", Value(Hex(iface_desc.iclass)));
							xml.attribute("protocol", Value(Hex(iface_desc.iprotocol)));
						});
					}
				}
			});
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


void ::Root::announce_device(genode_usb_vendor_id_t  vendor,
                             genode_usb_product_id_t product,
                             genode_usb_class_num_t  cla,
                             genode_usb_bus_num_t    bus,
                             genode_usb_dev_num_t    dev)
{
	for (auto &device : _devices) {
		if (device.constructed())
			continue;

		device.construct(vendor, product, cla, bus, dev);
		_announce_service();
		_report();

		_for_each_session([&] (genode_usb_session & s) {
			if (device->usb_session || !_matches(*device, s))
				return;
			device->usb_session = &s;
			s.notify();
		});
		return;
	}

	error("Could not announce driver for new USB device, no slot left!");
}


void ::Root::discontinue_device(genode_usb_bus_num_t bus,
                                genode_usb_dev_num_t dev)
{
	for (auto &device : _devices) {
		if (!device.constructed() ||
		    device->bus != bus ||
		    device->dev != dev)
			continue;

		if (device->usb_session) {
			device->usb_session->notify();
			device->usb_session->flush_packet_stream();
		}

		device.destruct();
		_report();
		return;
	}
}


genode_usb_session_handle_t ::Root::session(genode_usb_bus_num_t bus,
                                            genode_usb_dev_num_t dev)
{
	genode_usb_session * session = nullptr;
	_for_each_device([&] (Device & d) {
		if (d.bus == bus && d.dev == dev)
			session = d.usb_session;
	});

	return session ? session->_id.id() : 0;
}


template <typename FUNC>
void ::Root::session(genode_usb_session_handle_t id, FUNC const & fn)
{
	genode_usb_session * session = nullptr;

	_for_each_session([&] (genode_usb_session & s) {
		if (s._id.id() == id) session = &s;
	});

	/*
	 * We've to execute the functor outside the session iteration,
	 * because the functor might block and the actual session
	 * can be destroyed in the meantime, which will lead to
	 * a corrupted next() pointer.
	 */
	if (session) fn(*session);
}


bool ::Root::device_associated(genode_usb_session   * session,
                               genode_usb_bus_num_t & bus,
                               genode_usb_dev_num_t & dev)
{
	bool ret = false;
	_for_each_device([&] (Device & d) {
		if (d.usb_session == session) {
			bus = d.bus;
			dev = d.dev;
			ret = true;
		}
	});
	return ret;
}


void ::Root::handle_empty_sessions()
{
	_for_each_session([&] (genode_usb_session & s) {
		bool associated = false;
		_for_each_device([&] (Device & d) {
			if (d.usb_session == &s) associated = true; });
		if (!associated)
			s.flush_packet_stream();
	});
}


void ::Root::decrement_session_id(genode_usb_session_handle_t id)
{
	if (id > 0 && id <= MAX_SESSIONS)
		_session_ids[id-1]->dec();
}


::Root::Root(Env & env, Allocator & alloc, Signal_context_capability cap)
:
	Root_component<genode_usb_session>(env.ep(), alloc),
	_env(env), _sigh_cap(cap)
{
	/* Reserve id zero which is invalid */
	for (unsigned i = 0; i < MAX_SESSIONS; i++)
		_session_ids[i].construct((genode_usb_session_handle_t)(i+1));

	_config.sigh(_config_handler);
	_config_update();
}


extern "C" void genode_usb_init(genode_env                   * env_ptr,
                                genode_allocator             * alloc_ptr,
                                genode_signal_handler        * sigh_ptr,
                                genode_usb_rpc_callbacks     * callbacks)
{
	static ::Root root(*static_cast<Env*>(env_ptr),
	                   *static_cast<Allocator*>(alloc_ptr),
	                   cap(sigh_ptr));
	_callbacks = callbacks;
	_usb_root  = &root;
}


extern "C" void genode_usb_announce_device(genode_usb_vendor_id_t  vendor,
                                           genode_usb_product_id_t product,
                                           genode_usb_class_num_t  cla,
                                           genode_usb_bus_num_t    bus,
                                           genode_usb_dev_num_t    dev)
{
	if (!_usb_root)
		return;

	_usb_root->announce_device(vendor, product, cla, bus, dev);
}


extern "C" void genode_usb_discontinue_device(genode_usb_bus_num_t bus,
                                              genode_usb_dev_num_t dev)
{
	if (_usb_root)
		_usb_root->discontinue_device(bus, dev);
}


extern "C" genode_usb_session_handle_t
genode_usb_session_by_bus_dev(genode_usb_bus_num_t bus,
                              genode_usb_dev_num_t dev)
{
	genode_usb_session_handle_t ret = _usb_root ? _usb_root->session(bus, dev) : 0;
	return ret;
}


extern "C" int
genode_usb_request_by_session(genode_usb_session_handle_t id,
                              struct genode_usb_request_callbacks * const req,
                              void * data)
{
	bool ret = false;

	if (!_usb_root)
		return ret;

	_usb_root->session(id, [&] (genode_usb_session & session) {
		ret = session.request(*req, data); });
	return ret;
}


extern "C" void genode_usb_ack_request(genode_usb_session_handle_t session_id,
                                       genode_usb_request_handle_t request_id,
                                       genode_usb_response_t       callback,
                                       void *                      callback_data)
{
	if (!_usb_root)
		return;

	_usb_root->session(session_id, [&] (genode_usb_session & session) {
		session.handle_response(request_id, callback, callback_data); });

	_usb_root->decrement_session_id(session_id);
}


extern "C" void genode_usb_notify_peers() { }


extern "C" void genode_usb_handle_empty_sessions()
{
	if (!_usb_root)
		return;

	_usb_root->handle_empty_sessions();
}
