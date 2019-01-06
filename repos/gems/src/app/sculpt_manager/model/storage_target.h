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
	typedef String<Storage_device::Label::capacity() + 4> Label;

	Storage_device::Label device;
	Partition::Number     partition;

	bool operator == (Storage_target const &other) const
	{
		return (device == other.device) && (partition == other.partition);
	}

	bool operator != (Storage_target const &other) const { return !(*this == other); }

	bool valid() const { return device.valid(); }

	/**
	 * Return string to be used as session label referring to the target
	 */
	Label label() const
	{
		return partition.valid() ? Label(device, ".", partition) : Label(device);
	}

	bool ram_fs() const { return device == "ram_fs"; }

	Label fs() const { return ram_fs() ? label() : Label(label(), ".fs"); }

	void gen_block_session_route(Xml_generator &xml) const
	{
		bool const ahci = (Label(Cstring(device.string(), 4)) == "ahci");
		bool const nvme = (Label(Cstring(device.string(), 4)) == "nvme");
		bool const usb  = (Label(Cstring(device.string(), 3)) == "usb");

		bool const whole_device = !partition.valid();

		xml.node("service", [&] () {
			xml.attribute("name", Block::Session::service_name());

			if (whole_device) {

				if (ahci || nvme)
					xml.node("parent", [&] () { xml.attribute("label", device); });

				if (usb)
					xml.node("child", [&] () {
						xml.attribute("name",  Label(device, ".drv"));
						xml.attribute("label", partition);
					});
			}

			/* access partition */
			else {
				xml.node("child", [&] () {
					xml.attribute("name",  Label(device, ".part_block"));
					xml.attribute("label", partition);
				});
			}
		});
	}
};

#endif /* _MODEL__STORAGE_TARGET_H_ */
