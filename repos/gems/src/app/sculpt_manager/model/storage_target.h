/*
 * \brief  Argument type for denoting a storage target
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__STORAGE_TARGET_H_
#define _MODEL__STORAGE_TARGET_H_

#include "types.h"
#include "storage_device.h"

namespace Sculpt { struct Storage_target; }


struct Sculpt::Storage_target
{
	using Label = String<Storage_device::Label::capacity() + 5>;

	Storage_device::Label device;
	Storage_device::Port  port;
	Partition::Number     partition;

	bool operator == (Storage_target const &other) const
	{
		return (device    == other.device)
		    && (port      == other.port)
		    && (partition == other.partition);
	}

	bool operator != (Storage_target const &other) const { return !(*this == other); }

	bool valid() const { return device.valid(); }

	Label device_and_port() const
	{
		return port.valid() ? Label { device, "-", port } : Label { device };
	}

	/**
	 * Return string to be used as session label referring to the target
	 */
	Label label() const
	{
		return partition.valid() ? Label(device_and_port(), ".", partition)
		                         : Label(device_and_port());
	}

	bool ram_fs() const { return device == "ram_fs"; }

	Label fs() const { return ram_fs() ? label() : Label(label(), ".fs"); }

	void gen_block_session_route(Xml_generator &xml) const
	{
		bool const usb  = (Label(Cstring(device.string(), 3)) == "usb");
		bool const ahci = (Label(Cstring(device.string(), 4)) == "ahci");
		bool const nvme = (Label(Cstring(device.string(), 4)) == "nvme");
		bool const mmc  = (Label(Cstring(device.string(), 3)) == "mmc");

		bool const whole_device = !partition.valid();

		xml.node("service", [&] {
			xml.attribute("name", Block::Session::service_name());

			if (whole_device) {

				if (usb || ahci || nvme || mmc)
					xml.node("child", [&] {
						xml.attribute("name",  device);
						if (port.valid())
							xml.attribute("label", port);
					});
				else
					xml.node("parent", [&] { xml.attribute("label", device); });
			}

			/* access partition */
			else {
				xml.node("child", [&] {
					xml.attribute("name",  Label(device_and_port(), ".part"));
					xml.attribute("label", partition);
				});
			}
		});
	}
};

#endif /* _MODEL__STORAGE_TARGET_H_ */
