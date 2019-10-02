/*
 * \brief  VirtIO bus session interface
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

#include <session/session.h>
#include <virtio_device/capability.h>

namespace Virtio {
	struct Session;

	typedef Genode::Out_of_caps Out_of_caps;
}


namespace Virtio {
	enum Device_type {
		INVALID           = 0,
		NIC               = 1,
		BLOCK             = 2,
		CONSOLE           = 3,
		ENTROPY_SOURCE    = 4,
		MEMORY_BALLOONING = 5,
		IO_MEMORY         = 6,
		RPMSG             = 7,
		SCSI_HOST         = 8,
		TRANSPORT_9P      = 9,
		MAC80211_WLAN     = 10,
		RPROC_SERIAL      = 11,
		CAIF              = 12,
		MEMORY_BALLOON    = 13,
		GPU               = 16,
		TIMER             = 17,
		INPUT             = 18,
		UNKNOWN           = 19,
	};
}


struct Virtio::Session : Genode::Session
{
	/**
	 * Signal the client has claimed to many devices without calling release_device.
	 */
	struct Out_of_device_slots : Genode::Exception { };

	/**
	 * \noapi
	 */
	static const char *service_name() { return "VirtIO"; }

	enum {
		CAP_QUOTA = 8,

		/**
		 * Maximum number of VirtIO device capabilities each client can hold on to.
		 * Exceeding this number will result in Out_of_device_slots exception being
		 * thrown from both first_device and next_device calls.
		 */
		DEVICE_SLOT_COUNT = 2,
	};

	virtual ~Session() = default;

	/**
	 * Find first accessible device
	 *
	 * \param type  VirtIO device type to find.
	 *
	 * \throw Out_of_caps
	 * \throw Out_of_device_slots
	 */
	virtual Device_capability first_device(Device_type type) = 0;

	/**
	 * Find next accessible device
	 *
	 * \param prev_device previous device
	 *
	 * \throw Out_of_caps
	 * \throw Out_of_device_slots
	 */
	virtual Device_capability next_device(Device_capability prev_device) = 0;

	/**
	 * Release device and free resources allocated for it
	 *
	 * \param device  device to release
	 */
	virtual void release_device(Device_capability device) = 0;

	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_first_device, Device_capability, first_device,
	                 GENODE_TYPE_LIST(Out_of_device_slots, Out_of_caps),
			 Device_type);
	GENODE_RPC_THROW(Rpc_next_device, Device_capability, next_device,
	                 GENODE_TYPE_LIST(Out_of_device_slots, Out_of_caps),
			 Device_capability);
	GENODE_RPC(Rpc_release_device, void, release_device, Device_capability);

	GENODE_RPC_INTERFACE(Rpc_first_device, Rpc_next_device, Rpc_release_device);
};

namespace Genode {

	static inline void print(Output &output, Virtio::Device_type type)
	{
		switch (type) {
		default:                        print(output, "unknown");           break;
		case Virtio::INVALID:           print(output, "invalid");           break;
		case Virtio::NIC:               print(output, "NIC");               break;
		case Virtio::BLOCK:             print(output, "block");             break;
		case Virtio::CONSOLE:           print(output, "console");           break;
		case Virtio::ENTROPY_SOURCE:    print(output, "entropy source");    break;
		case Virtio::MEMORY_BALLOONING: print(output, "memory ballooning"); break;
		case Virtio::IO_MEMORY:         print(output, "IO memory");         break;
		case Virtio::RPMSG:             print(output, "Rpmsg");             break;
		case Virtio::SCSI_HOST:         print(output, "SCSI host");         break;
		case Virtio::TRANSPORT_9P:      print(output, "9P Transport");      break;
		case Virtio::MAC80211_WLAN:     print(output, "WiFi");              break;
		case Virtio::RPROC_SERIAL:      print(output, "rproc serial");      break;
		case Virtio::CAIF:              print(output, "caif");              break;
		case Virtio::MEMORY_BALLOON:    print(output, "memory balloon");    break;
		case Virtio::GPU:               print(output, "GPU");               break;
		case Virtio::TIMER:             print(output, "timer");             break;
		case Virtio::INPUT:             print(output, "input");             break;
		}
	}


}
