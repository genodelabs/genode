/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2014-03-03
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <util/xml_node.h>

/* local includes */
#include "wpa.h"

typedef long long ssize_t;

extern void wifi_init(Server::Entrypoint &, Genode::Lock &);
extern "C" void wpa_conf_reload(void);
extern "C" ssize_t wpa_write_conf(char const *, Genode::size_t);

bool config_verbose = false;

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
static int generate_wpa_supplicant_conf(char const **p, Genode::size_t *len, char const *ssid,
                       char const *bssid, bool protection = false, char const *psk = 0)
{
	static char const *start_fmt = "network={\n";
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
	Genode::Attached_rom_dataspace               config_rom { "wlan_configuration" };
	Genode::Signal_rpc_member<Wlan_configration> dispatcher;
	Genode::Lock                                 update_lock;

	char const     *buffer;
	Genode::size_t  size;

	/**
	 * Write configuration buffer to conf file to activate the new configuration.
	 */
	void _activate_configuration()
	{
		if (wpa_write_conf(buffer, size) == 0) {
			PINF("reload wpa_supplicant configuration");
			wpa_conf_reload();
		}
	}

	/**
	 * Write dummy configuration buffer to conf file to activate the new
	 * configuration.
	 */
	void _active_dummy_configuration()
	{
		generate_wpa_supplicant_conf(&buffer, &size, "dummyssid", "00:00:00:00:00:00");
		_activate_configuration();
	}

	/**
	 * Update the conf file used by the wpa_supplicant.
	 */
	void _update_configuration()
	{
		Genode::Lock::Guard guard(update_lock);

		config_rom.update();

		/**
		 * We generate a dummy configuration because there is no valid
		 * configuration yet to fool wpa_supplicant to keep it scanning
		 * for the non exisiting network.
		 */
		if (!config_rom.is_valid()) {
			_active_dummy_configuration();
			return;
		}

		Genode::Xml_node node(config_rom.local_addr<char>(), config_rom.size());

		/**
		 * Since <selected_accesspoint/> is empty we also generate a dummy
		 * configuration.
		 */
		bool use_ssid  = node.has_attribute("ssid");
		bool use_bssid = node.has_attribute("bssid");
		if (!use_ssid && !use_bssid) {
			_active_dummy_configuration();
			return;
		}

		/**
		 * Try to generate a valid configuration.
		 */
		enum { MAX_SSID_LENGTH = 32,
		       BSSID_LENGTH    = 12 + 5,
		       MIN_PSK_LENGTH  =  8,
		       MAX_PSK_LENGTH  = 63 };

		char ssid[MAX_SSID_LENGTH + 1] = { 0 };
		if (use_ssid)
			node.attribute("ssid").value(ssid, sizeof(ssid));

		char bssid[BSSID_LENGTH + 1] = { 0 };
		if (use_bssid)
			node.attribute("bssid").value(bssid, sizeof(bssid));

		bool use_protection = node.has_attribute("protection");

		char psk[MAX_PSK_LENGTH + 1] = { 0 };
		if (use_protection && node.has_attribute("psk"))
			node.attribute("psk").value(psk, sizeof(psk));

		/* psk must be between 8 and 63 characters long */
		if (use_protection && (Genode::strlen(psk) < MIN_PSK_LENGTH)) {
			_active_dummy_configuration();
			return;
		}

		if (generate_wpa_supplicant_conf(&buffer, &size,
		                                 use_ssid ? ssid : 0,
		                                 use_bssid ? bssid : 0,
		                                 use_protection, psk) == 0)
			_activate_configuration();
	}

	void _handle_update(unsigned) { _update_configuration(); }

	Wlan_configration(Server::Entrypoint &ep)
	:
		dispatcher(ep, *this, &Wlan_configration::_handle_update)
	{
		config_rom.sigh(dispatcher);
		_update_configuration();
	}
};


struct Main
{
	Server::Entrypoint &_ep;
	Wpa_thread         *_wpa;

	Wlan_configration   *_wc;

	Main(Server::Entrypoint &ep)
	:
		_ep(ep)
	{
		try {
			config_verbose = Genode::config()->xml_node().attribute("verbose").has_value("yes");
		} catch (...) { }
		_wpa = new (Genode::env()->heap()) Wpa_thread(wpa_startup_lock());

		_wpa->start();

		try {
			_wc = new (Genode::env()->heap()) Wlan_configration(_ep);
		} catch (...) { PWRN("could not create wlan_configration handler"); }

		wifi_init(ep, wpa_startup_lock());
	}
};


namespace Server {
	char const *name()             { return "wifi_drv_ep";   }
	size_t      stack_size()       { return 32 * 1024 * sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep); }
}
