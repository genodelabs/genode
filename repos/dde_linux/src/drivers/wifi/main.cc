/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/attached_rom_dataspace.h>
#include <util/xml_node.h>
#include <util/string.h>

/* local includes */
#include "wpa.h"

typedef long long ssize_t;

extern void wifi_init(Genode::Env&, Genode::Lock&);
extern "C" void wpa_conf_reload(void);
extern "C" ssize_t wpa_write_conf(char const *, Genode::size_t);

static Genode::Lock &wpa_startup_lock()
{
	static Genode::Lock _l(Genode::Lock::LOCKED);
	return _l;
}


namespace {
	template <Genode::size_t CAPACITY>
	class Buffer
	{
		private:

			char           _data[CAPACITY] { 0 };
			Genode::size_t _length { 0 };

		public:

			void            reset()       { _data[0] = 0; _length = 0; }
			char const      *data() const { return _data; }
			Genode::size_t length() const { return _length; }

			void append(char const *format, ...)
			{
				va_list list;
				va_start(list, format);
				Genode::String_console sc(_data + _length, CAPACITY - _length);
				sc.vprintf(format, list);
				va_end(list);

				_length += sc.len();
			}
	};
} /* anonymous namespace */


/**
 * Generate wpa_supplicant.conf file
 */
static int generatewpa_supplicant_conf(char const **p, Genode::size_t *len, char const *ssid,
                       char const *bssid, bool protection = false, char const *psk = 0)
{
	static char const *start_fmt = "network={\n\tscan_ssid=1\n";
	static char const *ssid_fmt  = "\tssid=\"%s\"\n";
	static char const *bssid_fmt = "\tbssid=%s\n";
	static char const *prot_fmt  = "\tkey_mgmt=%s\n";
	static char const *psk_fmt   = "\tpsk=\"%s\"\n";
	static char const *end_fmt   = "}\n";

	static Buffer<256> buffer;
	buffer.reset();

	buffer.append(start_fmt);

	if (ssid)
		buffer.append(ssid_fmt, ssid);

	if (bssid)
		buffer.append(bssid_fmt, bssid);

	if (protection)
		buffer.append(psk_fmt, psk);

	buffer.append(prot_fmt, protection ? "WPA-PSK" : "NONE");

	buffer.append(end_fmt);

	*p = buffer.data();
	*len = buffer.length();

	return 0;
}


struct Wlan_configration
{
	Genode::Attached_rom_dataspace            config_rom { "wlan_configuration" };
	Genode::Signal_handler<Wlan_configration> dispatcher;
	Genode::Lock                              update_lock;

	char const     *buffer;
	Genode::size_t  size;

	/**
	 * Write configuration buffer to conf file to activate the new configuration.
	 */
	void _activate_configuration()
	{
		if (wpa_write_conf(buffer, size) == 0) {
			Genode::log("Reload wpa_supplicant configuration");
			wpa_conf_reload();
		}
	}

	/**
	 * Write dummy configuration buffer to conf file to activate the new
	 * configuration.
	 */
	void _active_dummy_configuration()
	{
		generatewpa_supplicant_conf(&buffer, &size, "dummyssid", "00:00:00:00:00:00");
		_activate_configuration();
	}

	/**
	 * Update the conf file used by the wpa_supplicant.
	 */
	void _update_configuration()
	{
		using namespace Genode;

		Lock::Guard guard(update_lock);

		config_rom.update();

		/**
		 * We generate a dummy configuration because there is no valid
		 * configuration yet to fool wpa_supplicant to keep it scanning
		 * for the non exisiting network.
		 */
		if (!config_rom.valid()) {
			_active_dummy_configuration();
			return;
		}

		Xml_node node(config_rom.local_addr<char>(), config_rom.size());

		/**
		 * Since <selected_accesspoint/> is empty or missing an ssid attribute
		 * we also generate a dummy configuration.
		 */
		if (!node.has_attribute("ssid")) {
			_active_dummy_configuration();
			return;
		}

		/**
		 * Try to generate a valid configuration.
		 */
		enum { MAX_SSID_LENGTH = 32 + 1,
		       BSSID_LENGTH    = 12 + 5 + 1,
		       PROT_LENGTH     = 7 + 1,
		       MIN_PSK_LENGTH  = 8,
		       MAX_PSK_LENGTH  = 63 + 1};

		String<MAX_SSID_LENGTH> ssid;
		node.attribute("ssid").value(&ssid);

		bool use_bssid = node.has_attribute("bssid");
		String<BSSID_LENGTH> bssid;
		if (use_bssid)
			node.attribute("bssid").value(&bssid);

		bool use_protection = false;
		if (node.has_attribute("protection")) {
			String<PROT_LENGTH> prot;
			node.attribute("protection").value(&prot);
			use_protection = (prot == "WPA-PSK");
		}

		String<MAX_PSK_LENGTH> psk;
		if (use_protection && node.has_attribute("psk"))
			node.attribute("psk").value(&psk);

		/* psk must be between 8 and 63 characters long */
		if (use_protection && (psk.length() < MIN_PSK_LENGTH)) {
			Genode::error("given pre-shared key is too short");
			_active_dummy_configuration();
			return;
		}

		if (generatewpa_supplicant_conf(&buffer, &size, ssid.string(),
		                                 use_bssid ? bssid.string() : 0,
		                                 use_protection, psk.string()) == 0)
			_activate_configuration();
	}

	void _handle_update() { _update_configuration(); }

	Wlan_configration(Genode::Entrypoint &ep)
	:
		dispatcher(ep, *this, &Wlan_configration::_handle_update)
	{
		config_rom.sigh(dispatcher);
		_update_configuration();
	}
};


struct Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Genode::Attached_rom_dataspace config_rom { env, "config" };

	Wpa_thread        *wpa;
	Wlan_configration *wlan_config;

	Main(Genode::Env &env) : env(env)
	{
		bool const verbose = config_rom.xml().attribute_value("verbose", false);

		wpa = new (&heap) Wpa_thread(env, wpa_startup_lock(), verbose);

		wpa->start();

		try {
			wlan_config = new (&heap) Wlan_configration(env.ep());
		} catch (...) {
			Genode::warning("could not create Wlan_configration handler");
		}

		wifi_init(env, wpa_startup_lock());
	}
};


namespace Component {
	Genode::size_t      stack_size() { return 32 * 1024 * sizeof(long); }
	void construct(Genode::Env &env) { static Main server(env);         }
}
