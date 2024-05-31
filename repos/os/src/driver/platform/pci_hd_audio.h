/*
 * \brief  Platform driver - PCI HD AUDIO utilities
 * \author Stefan Kalkowski
 * \date   2022-09-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pci/config.h>
#include <device.h>

namespace Driver {
	static void pci_hd_audio_quirks(Device::Pci_config const &, Pci::Config &);
}


void Driver::pci_hd_audio_quirks(Device::Pci_config const & cfg, Pci::Config & config)
{
	enum { HDAUDIO_CLASS_CODE = 0x40300 };

	if ((cfg.class_code & 0xffff00) != HDAUDIO_CLASS_CODE)
		return;

	/* PCI configuration register for HDAUDIO */
	struct Hdaudio : Mmio<0x7a>
	{
		struct Traffic_class_select : Register<0x44, 8> {};

		struct Intel_device_control : Register<0x78, 16>
		{
			struct No_snoop : Bitfield<11,1> {};
		};

		struct Amd_device_control : Register<0x42, 8>
		{
			struct No_snoop : Bitfield<0, 3> {};
		};

		using Mmio::Mmio;
	};

	config.write<Pci::Config::Command::Fast_back_to_back_enable>(1);

	Hdaudio audio(config.range());
	audio.write<Hdaudio::Traffic_class_select>(0);

	if (cfg.vendor_id == 0x8086)
		audio.write<Hdaudio::Intel_device_control::No_snoop>(0);

	if (cfg.vendor_id == 0x1002 || cfg.vendor_id == 0x1022)
		audio.write<Hdaudio::Amd_device_control::No_snoop>(2);
}

