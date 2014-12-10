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
#include <os/server.h>
#include <util/xml_node.h>

/* local includes */
#include "wpa.h"

typedef long long ssize_t;

extern void wifi_init(Server::Entrypoint &, Genode::Lock &);
extern "C" void wpa_conf_reload(void);
extern "C" ssize_t wpa_write_conf(char const *, Genode::size_t);


static Genode::Lock &wpa_startup_lock()
{
	static Genode::Lock _l(Genode::Lock::LOCKED);
	return _l;
}


static int update_conf(char **p, Genode::size_t *len, char const *ssid,
                       bool encryption = false, char const *psk = 0)
{
	char const *ssid_fmt = "network={\n\tssid=\"%s\"\n\tkey_mgmt=%s\n";
	char const *psk_fmt  = "psk=\"%s\"\n";
	char const *end_fmt  = "}\n";

	static char buf[4096];
	Genode::size_t n = Genode::snprintf(buf, sizeof(buf), ssid_fmt, ssid,
	                                    encryption ? "WPA-PSK" : "NONE");
	if (encryption) {
		n += Genode::snprintf(buf + n, sizeof(buf) - n, psk_fmt, psk);
	}

	n += Genode::snprintf(buf + n, sizeof(buf) - n, end_fmt);

	*p = buf;
	*len = n;

	return 0;
}


struct Wlan_configration
{
	Genode::Attached_rom_dataspace              config_rom { "wlan_configuration" };
	Genode::Signal_rpc_member<Wlan_configration> dispatcher;

	void _update_conf()
	{
		config_rom.update();
		Genode::size_t size = config_rom.size();

		char           *file_buffer;
		Genode::size_t  buffer_length;

		Genode::Xml_node node(config_rom.local_addr<char>(), size);

		/**
		 * Since <selected_accesspoint/> is empty we generate a dummy
		 * configuration to fool wpa_supplicant to keep it scanning for
		 * the non exisiting network.
		 */
		if (!node.has_attribute("ssid")) {
			update_conf(&file_buffer, &buffer_length, "dummyssid");
		} else {
			char ssid[32+1] = { 0 };
			node.attribute("ssid").value(ssid, sizeof(ssid));

			bool use_protection = node.has_attribute("protection");

			char psk[63+1] = { 0 };
			if (use_protection && node.has_attribute("psk"))
				node.attribute("psk").value(psk, sizeof(psk));

			if (update_conf(&file_buffer, &buffer_length, ssid,
			                use_protection, psk) != 0)
				return;
			}

		if (wpa_write_conf(file_buffer, buffer_length) == 0) {
			PINF("reload wpa_supplicant configuration");
			wpa_conf_reload();
		}
	}

	void _handle_update(unsigned) { _update_conf(); }

	Wlan_configration(Server::Entrypoint &ep)
	:
		dispatcher(ep, *this, &Wlan_configration::_handle_update)
	{
		config_rom.sigh(dispatcher);
		_update_conf();
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
