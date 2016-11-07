/*
 * \brief  WPA Supplicant frontend
 * \author Josef Soentgen
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <os/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <util/string.h>

/* WPA Supplicant includes */
extern "C" {
#include "includes.h"
#include "common.h"
#include "drivers/driver.h"
#include "wpa_supplicant_i.h"
#include "bss.h"
#include "scan.h"
#include "common/ieee802_11_defs.h"
}


static Genode::Reporter accesspoints_reporter = { "wlan_accesspoints" };
static Genode::Reporter state_reporter        = { "wlan_state" };


enum { SSID_MAX_LEN = 32 + 1, MAC_STR_LEN = 6*2 + 5 + 1};


static inline void mac2str(char *buf, u8 const *mac)
{
	Genode::snprintf(buf, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
	                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


extern "C" void wpa_report_connect_event(struct wpa_supplicant *wpa_s)
{
	state_reporter.enabled(true);

	try {
		Genode::Reporter::Xml_generator xml(state_reporter, [&]() {
			struct wpa_ssid *wpa_ssid = wpa_s->current_ssid;

			/* FIXME ssid may contain any characters, even NUL */
			Genode::String<SSID_MAX_LEN>
				ssid(Genode::Cstring((char *)wpa_ssid->ssid, wpa_ssid->ssid_len));

			char bssid_buf[MAC_STR_LEN];
			mac2str(bssid_buf, wpa_s->bssid);

			xml.node("accesspoint", [&]() {
				xml.attribute("ssid", ssid.string());
				xml.attribute("bssid", bssid_buf);
				xml.attribute("state", "connected");
			});
		});
	} catch (...) { Genode::warning("could not report connected state"); }
}


extern "C" void wpa_report_disconnect_event(struct wpa_supplicant *wpa_s)
{
	state_reporter.enabled(true);

	try {
		Genode::Reporter::Xml_generator xml(state_reporter, [&]() {
			struct wpa_ssid *wpa_ssid = wpa_s->current_ssid;

			/* FIXME ssid may contain any characters, even NUL */
			Genode::String<SSID_MAX_LEN>
				ssid(Genode::Cstring((char *)wpa_ssid->ssid, wpa_ssid->ssid_len));

			char bssid_buf[MAC_STR_LEN];
			mac2str(bssid_buf, wpa_ssid->bssid);

			xml.node("accesspoint", [&]() {
				xml.attribute("ssid", ssid.string());
				xml.attribute("bssid", bssid_buf);
				xml.attribute("state", "disconnected");
			});

		});
	} catch (...) { Genode::warning("could not report disconnected state"); }
}


static inline int approximate_quality(struct wpa_bss *bss)
{
	/*
	 * We provide an quality value by transforming the actual
	 * signal level [-50,-100] (dBm) to [100,0] (%).
	 */
	int level = bss->level;

	if (level <= -100)
		return 0;
	else if (level >= -50)
		return 100;

	return 2 * (level + 100);
}


extern "C" void wpa_report_scan_results(struct wpa_supplicant *wpa_s)
{
	accesspoints_reporter.enabled(true);

	try {
		Genode::Reporter::Xml_generator xml(accesspoints_reporter, [&]() {
			for (unsigned i = 0; i < wpa_s->last_scan_res_used; i++) {
				struct wpa_bss *bss = wpa_s->last_scan_res[i];

				bool wpa  = wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE) != NULL;
				bool wpa2 = wpa_bss_get_ie(bss, WLAN_EID_RSN) != NULL;

				char bssid_buf[MAC_STR_LEN];
				mac2str(bssid_buf, bss->bssid);

				Genode::String<SSID_MAX_LEN>
					ssid(Genode::Cstring((char *)bss->ssid, bss->ssid_len));

				int quality = approximate_quality(bss);

				xml.node("accesspoint", [&]() {
					xml.attribute("ssid", ssid.string());
					xml.attribute("bssid", bssid_buf);
					xml.attribute("quality", quality);

					/* XXX we forcefully only support WPA/WPA2 psk for now */
					if (wpa || wpa2)
						xml.attribute("protection", "WPA-PSK");
				});
			}
		});
	} catch (...) { Genode::warning("could not report scan results"); }
}
