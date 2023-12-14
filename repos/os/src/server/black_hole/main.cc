/*
 * \brief  Black hole component
 * \author Christian Prochaska
 * \date   2021-07-07
 *
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <root/component.h>

/* local includes */
#include "audio_in.h"
#include "audio_out.h"
#include "capture.h"
#include "event.h"
#include "nic.h"
#include "uplink.h"
#include "uplink_client.h"
#include "report.h"
#include "rom.h"
#include "gpu.h"
#include "usb.h"


/***************
 ** Component **
 ***************/

namespace Black_hole { struct Main; }

struct Black_hole::Main
{
	Genode::Env &env;

	Genode::Sliced_heap heap { env.ram(), env.rm() };

	Genode::Constructible<Audio_in::Root>  audio_in_root  { };
	Genode::Constructible<Audio_out::Root> audio_out_root { };
	Genode::Constructible<Capture::Root>   capture_root   { };
	Genode::Constructible<Event_root>      event_root     { };
	Genode::Constructible<Nic_root>        nic_root       { };
	Genode::Constructible<Uplink_root>     uplink_root    { };
	Genode::Constructible<Report_root>     report_root    { };
	Genode::Constructible<Rom_root>        rom_root       { };
	Genode::Constructible<Gpu_root>        gpu_root       { };
	Genode::Constructible<Usb_root>        usb_root       { };
	Genode::Constructible<Uplink_client>   uplink_client  { };

	Main(Genode::Env &env) : env(env)
	{
		Genode::Attached_rom_dataspace _config_rom { env, "config" };

		if (_config_rom.xml().has_sub_node("audio_in")) {
			audio_in_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*audio_in_root));
		}
		if (_config_rom.xml().has_sub_node("audio_out")) {
			audio_out_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*audio_out_root));
		}
		if (_config_rom.xml().has_sub_node("capture")) {
			capture_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*capture_root));
		}
		if (_config_rom.xml().has_sub_node("event")) {
			event_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*event_root));
		}
		if (_config_rom.xml().has_sub_node("nic")) {
			nic_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*nic_root));
		}
		if (_config_rom.xml().has_sub_node("uplink")) {
			uplink_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*uplink_root));
		}
		if (_config_rom.xml().has_sub_node("rom")) {
			rom_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*rom_root));
		}
		if (_config_rom.xml().has_sub_node("report")) {
			report_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*report_root));
		}
		if (_config_rom.xml().has_sub_node("gpu")) {
			gpu_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*gpu_root));
		}
		if (_config_rom.xml().has_sub_node("usb")) {
			usb_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*usb_root));
		}

		if (_config_rom.xml().has_sub_node("uplink_client")) {
			uplink_client.construct(env, heap);
		}
	}
};


void Component::construct(Genode::Env &env)
{ static Black_hole::Main inst(env); }
