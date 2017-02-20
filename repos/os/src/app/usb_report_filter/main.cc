/*
 * \brief  Component that filters USB device reports
 * \author Josef Soentgen
 * \date   2016-01-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <util/list.h>
#include <util/string.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>


namespace Usb_filter {

	using Genode::Xml_node;
	using Genode::Xml_generator;
	using Genode::Attached_rom_dataspace;
	using Genode::snprintf;
	using Genode::error;
	using Genode::log;
	using Genode::warning;

	struct Device_registry;
	struct Main;
}

static bool const verbose = false;

static char const * const config_file = "usb_drv.config";


class Usb_filter::Device_registry
{
	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		Genode::Reporter   _reporter { _env, "usb_devices" };

		Attached_rom_dataspace _devices_rom        { _env, "devices" };
		Attached_rom_dataspace _usb_drv_config_rom { _env, "usb_drv_config" };

		Genode::Allocator_avl    _fs_packet_alloc { &_alloc };
		File_system::Connection  _fs              { _env, _fs_packet_alloc, "usb_drv.config" };
		File_system::File_handle _file;

		struct Entry : public Genode::List<Entry>::Element
		{
			unsigned bus;
			unsigned dev;
			unsigned vendor;
			unsigned product;

			Entry(unsigned b, unsigned d, unsigned v, unsigned p)
			: bus(b), dev(d), vendor(v), product(p) { }
		};

		Genode::List<Entry> _list;

		enum { MAX_LABEL_LEN = 512 };
		typedef Genode::String<MAX_LABEL_LEN> Label;
		Label _client_label;

		template <typename FUNC>
		void _for_each_entry(FUNC const &func) const
		{
			Entry const *e    = _list.first();
			Entry const *next = nullptr;
			for (; e; e = next) {

				/*
				 * Obtain next element prior calling the functor because
				 * the functor may remove the current element from the list.
				 */
				next = e->next();

				func(*e);
			}
		}

		static inline unsigned _get_value(Xml_node node, char const * const attr) {
			return node.attribute_value<unsigned long>(attr, 0); }

		static bool _config_has_device(Xml_node config, Entry const &entry)
		{
			bool result = false;
			config.for_each_sub_node("device", [&] (Xml_node usb_device) {

				result |= (_get_value(usb_device, "bus") == entry.bus &&
				           _get_value(usb_device, "dev") == entry.dev);
				if (result) return;

				result |= (_get_value(usb_device, "vendor")  == entry.vendor &&
				           _get_value(usb_device, "product") == entry.product);
			});

			return result;
		}

		static bool _devices_matches(Xml_node &device, Entry const & entry)
		{
			unsigned const bus     = _get_value(device, "bus");
			unsigned const dev     = _get_value(device, "dev");
			unsigned const vendor  = _get_value(device, "vendor_id");
			unsigned const product = _get_value(device, "product_id");

			return (bus == entry.bus && dev == entry.dev) ||
			       (vendor == entry.vendor && product == entry.product);
		}

		static void _gen_policy_entry(Xml_generator &xml, Xml_node &node,
		                        Entry const &entry, char const *label)
		{
			xml.node("policy", [&] {
				char buf[MAX_LABEL_LEN + 16];

				unsigned const bus = _get_value(node, "bus");
				unsigned const dev = _get_value(node, "dev");

				snprintf(buf, sizeof(buf), "%s -> usb-%d-%d", label, bus, dev);
				xml.attribute("label", buf);

				snprintf(buf, sizeof(buf), "0x%4x", _get_value(node, "vendor_id"));
				xml.attribute("vendor_id", buf);

				snprintf(buf, sizeof(buf), "0x%4x", _get_value(node, "product_id"));
				xml.attribute("product_id", buf);

				snprintf(buf, sizeof(buf), "0x%4x", bus);
				xml.attribute("bus", buf);

				snprintf(buf, sizeof(buf), "0x%4x", dev);
				xml.attribute("dev", buf);
			});
		}

		void _write_usb_drv_config(Xml_node & usb_devices)
		{
			using namespace Genode;

			try {
				File_system::Dir_handle root_dir = _fs.dir("/", false);
				_file = _fs.file(root_dir, config_file, File_system::READ_WRITE, false);
			} catch (...) {
				error("could not open '", config_file, "'");
				return;
			}

			char old_file[1024];
			size_t n = File_system::read(_fs, _file, old_file,
			                                   sizeof(old_file));
			if (n == 0) {
				error("could not read '", config_file, "'");
				return;
			}

			Xml_node drv_config(old_file, n);

			bool const uhci_enabled    = drv_config.attribute_value<bool>("uhci", false);
			bool const ehci_enabled    = drv_config.attribute_value<bool>("ehci", false);
			bool const xhci_enabled    = drv_config.attribute_value<bool>("xhci", false);

			char new_file[1024];
			Genode::Xml_generator xml(new_file, sizeof(new_file), "config", [&] {
				if (uhci_enabled) xml.attribute("uhci", "yes");
				if (ehci_enabled) xml.attribute("ehci", "yes");
				if (xhci_enabled) xml.attribute("xhci", "yes");

				/* copy other nodes */
				drv_config.for_each_sub_node([&] (Xml_node &node) {
					if (!node.has_type("raw")) {
						xml.append(node.addr(), node.size());
						return;
					}
				});

				if (!drv_config.has_sub_node("raw"))
					log("enable raw support in usb_drv");

				xml.node("raw", [&] {
					xml.node("report", [&] {
						xml.attribute("devices", "yes");
					});

					char const * const label = _client_label.string();

					usb_devices.for_each_sub_node("device", [&] (Xml_node &node) {

						auto add_policy_entry = [&] (Entry const &entry) {
							if (!_devices_matches(node, entry)) return;

							_gen_policy_entry(xml, node, entry, label);
						};
						_for_each_entry(add_policy_entry);
					});
				});
			});

			new_file[xml.used()] = 0;
			if (verbose)
				log("new usb_drv configuration:\n", Cstring(new_file));

			n = File_system::write(_fs, _file, new_file, xml.used());
			if (n == 0)
				error("could not write '", config_file, "'");

			_fs.close(_file);
		}

		Genode::Signal_handler<Device_registry> _devices_handler =
		{ _env.ep(), *this, &Device_registry::_handle_devices };

		void _handle_devices()
		{
			_devices_rom.update();

			if (!_devices_rom.valid()) return;

			if (verbose)
				log("device report:\n", _devices_rom.local_addr<char const>());

			Xml_node usb_devices(_devices_rom.local_addr<char>(), _devices_rom.size());

			_write_usb_drv_config(usb_devices);
		}

		bool _check_config(Xml_node &drv_config)
		{
			if (!drv_config.has_sub_node("raw")) {
				error("could not access <raw> node");
				return false;
			}

			auto check_policy = [&] (Entry const &entry) {
				bool result = false;
				drv_config.sub_node("raw").for_each_sub_node("policy", [&] (Xml_node &node) {
						result |= (entry.bus == _get_value(node, "bus") &&
						           entry.dev == _get_value(node, "dev"));
						if (result) return;

						result |= (entry.vendor  == _get_value(node, "vendor_id") &&
						           entry.product == _get_value(node, "product_id"));
				});

				if (verbose && !result)
					warning("No matching policy was created for "
					        "device ", entry.bus, "-", entry.dev, " "
					        "(", entry.vendor, ":", entry.product, ")");
			};
			_for_each_entry(check_policy);

			return true;
		}

		static void _gen_device_entry(Xml_generator &xml, Xml_node &node,
		                              Entry const &entry)
		{
			xml.node("device", [&] {
				char buf[16];

				unsigned const bus = _get_value(node, "bus");
				unsigned const dev = _get_value(node, "dev");

				snprintf(buf, sizeof(buf), "usb-%d-%d", bus, dev);
				xml.attribute("label", buf);

				snprintf(buf, sizeof(buf), "0x%4x", _get_value(node, "vendor_id"));
				xml.attribute("vendor_id", buf);

				snprintf(buf, sizeof(buf), "0x%4x", _get_value(node, "product_id"));
				xml.attribute("product_id", buf);

				snprintf(buf, sizeof(buf), "0x%4x", bus);
				xml.attribute("bus", buf);

				snprintf(buf, sizeof(buf), "0x%4x", dev);
				xml.attribute("dev", buf);
			});
		}

		void _report_usb_devices()
		{
			using namespace Genode;

			/*
			 * XXX it might happen that the device list has changed after we are
			 *     waiting for the usb_drv_config update
			 */
			Xml_node usb_devices(_devices_rom.local_addr<char>(), _devices_rom.size());

			Reporter::Xml_generator xml(_reporter, [&] () {
				usb_devices.for_each_sub_node("device", [&] (Xml_node &node) {

					auto check_entry = [&] (Entry const &entry) {
						if (!_devices_matches(node, entry)) return;

						_gen_device_entry(xml, node, entry);
					};
					_for_each_entry(check_entry);
				});
			});
		}

		Genode::Signal_handler<Device_registry> _usb_drv_config_handler =
		{ _env.ep(), *this, &Device_registry::_handle_usb_drv_config };

		void _handle_usb_drv_config()
		{
			_usb_drv_config_rom.update();

			if (!_usb_drv_config_rom.valid()) return;

			Xml_node config(_usb_drv_config_rom.local_addr<char>(),
			                _usb_drv_config_rom.size());

			if (!_check_config(config)) return;

			/* report devices if the USB drivers has changed its policies */
			_report_usb_devices();
		}

		bool _entry_exists(unsigned bus, unsigned dev,
		                   unsigned vendor, unsigned product)
		{
			bool result = false;
			auto check_exists = [&] (Entry const &entry) {
				result |= (bus && dev) &&
				          (entry.bus == bus && entry.dev == dev);
				result |= (vendor && product) &&
				          (entry.vendor == vendor && entry.product == product);
			};

			_for_each_entry(check_exists);

			return result;
		}


	public:

		/**
		 * Constructor
		 */
		Device_registry(Genode::Env &env, Genode::Allocator &alloc)
		:  _env(env), _alloc(alloc)
		{
			_reporter.enabled(true);

			_devices_rom.sigh(_devices_handler);

			_usb_drv_config_rom.sigh(_usb_drv_config_handler);
		}

		/**
		 * Update internal device registry
		 */
		void update_entries(Genode::Xml_node config)
		{
			auto remove_stale_entry = [&] (Entry const &entry) {

				if (_config_has_device(config, entry))
					return;

				_list.remove(const_cast<Entry *>(&entry));
				Genode::destroy(&_alloc, const_cast<Entry *>(&entry));
			};
			_for_each_entry(remove_stale_entry);

			auto add_new_entry = [&] (Xml_node const &node) {

				unsigned const bus     = _get_value(node, "bus");
				unsigned const dev     = _get_value(node, "dev");
				unsigned const vendor  = _get_value(node, "vendor_id");
				unsigned const product = _get_value(node, "product_id");

				if (_entry_exists(bus, dev, vendor, product)) return;

				Entry *entry = new (&_alloc) Entry(bus, dev, vendor, product);

				_list.insert(entry);
			};
			config.for_each_sub_node("device", add_new_entry);

			try {
				config.sub_node("client").attribute("label").value(&_client_label);
			} catch (...) {
				error("could not update client label");
			}
		}
};


struct Usb_filter::Main
{
	Genode::Env &_env;

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Genode::Signal_handler<Main> _config_handler =
	{ _env.ep(), *this, &Main::_handle_config };

	void _handle_config()
	{
		_config.update();
		device_registry.update_entries(_config.xml());
	}

	Device_registry device_registry { _env, _heap };

	Main(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		_handle_config();
	}
};


void Component::construct(Genode::Env &env) {
	static Usb_filter::Main main(env); }
