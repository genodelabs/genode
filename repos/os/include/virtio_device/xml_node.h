/*
 * \brief  Xml-node routines used internally in VirtIO bus driver
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <util/string.h>
#include <virtio_session/virtio_session.h>

namespace Genode {

	/**
	 * Convert ASCII string to VirtIO device type
	 */
	inline size_t ascii_to(char const *s, Virtio::Device_type &type)
	{
		using namespace Genode;

		if (!strcmp(s, "nic", 3)) {
			type = Virtio::Device_type::NIC;
			return 3;
		} else if (!strcmp(s, "block", 5)) {
			type = Virtio::Device_type::BLOCK;
			return 5;
		} else if (!strcmp(s, "console", 7)) {
			type = Virtio::Device_type::CONSOLE;
			return 7;
		} else if (!strcmp(s, "entropy", 7)) {
			type = Virtio::Device_type::ENTROPY_SOURCE;
			return 7;
		} else if (!strcmp(s, "memory_balooning", 16)) {
			type = Virtio::Device_type::MEMORY_BALLOONING;
			return 16;
		} else if (!strcmp(s, "io_memory", 9)) {
			type = Virtio::Device_type::IO_MEMORY;
			return 9;
		} else if (!strcmp(s, "Rpmsg", 5)) {
			type = Virtio::Device_type::RPMSG;
			return 5;
		} else if (!strcmp(s, "scsi_host", 9)) {
			type = Virtio::Device_type::SCSI_HOST;
			return 9;
		} else if (!strcmp(s, "9p_transport", 12)) {
			type = Virtio::Device_type::TRANSPORT_9P;
			return 12;
		} else if (!strcmp(s, "wifi", 4)) {
			type = Virtio::Device_type::MAC80211_WLAN;
			return 4;
		} else if (!strcmp(s, "rproc_serial", 12)) {
			type = Virtio::Device_type::RPROC_SERIAL;
			return 12;
		} else if (!strcmp(s, "caif", 4)) {
			type = Virtio::Device_type::CAIF;
			return 4;
		} else if (!strcmp(s, "memory_baloon", 13)) {
			type = Virtio::Device_type::MEMORY_BALLOON;
			return 13;
		} else if (!strcmp(s, "gpu", 3)) {
			type = Virtio::Device_type::GPU;
			return 3;
		} else if (!strcmp(s, "timer", 5)) {
			type = Virtio::Device_type::TIMER;
			return 5;
		} else if (!strcmp(s, "input", 5)) {
			type = Virtio::Device_type::INPUT;
			return 5;
		}

		return 0;
	}
}
