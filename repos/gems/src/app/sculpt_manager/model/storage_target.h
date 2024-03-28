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
	using Label = String<Storage_device::Driver::capacity() + 5>;

	Storage_device::Driver driver;
	Storage_device::Port   port;
	Partition::Number      partition;

	static Storage_target from_xml(Xml_node const &target)
	{
		return {
			.driver    = target.attribute_value("driver",    Storage_device::Driver()),
			.port      = target.attribute_value("port",      Storage_device::Port()),
			.partition = target.attribute_value("partition", Partition::Number())
		};
	}

	bool operator == (Storage_target const &other) const
	{
		return (driver    == other.driver)
		    && (port      == other.port)
		    && (partition == other.partition);
	}

	bool operator != (Storage_target const &other) const { return !(*this == other); }

	bool valid() const { return driver.valid(); }

	Label driver_and_port() const
	{
		return port.valid() ? Label { driver, "-", port } : Label { driver };
	}

	/**
	 * Return string to be used as session label referring to the target
	 */
	Label label() const
	{
		return partition.valid() ? Label(driver_and_port(), ".", partition)
		                         : Label(driver_and_port());
	}

	bool ram_fs() const { return driver == "ram_fs"; }

	Label fs() const { return ram_fs() ? label() : Label(label(), ".fs"); }

	void gen_block_session_route(Xml_generator &xml) const
	{
		bool const whole_device = !partition.valid();

		xml.node("service", [&] {
			xml.attribute("name", Block::Session::service_name());

			if (whole_device) {
				xml.node("child", [&] {
					xml.attribute("name",  driver);
					if (port.valid())
						xml.attribute("label", port); });
			}

			/* access partition */
			else {
				xml.node("child", [&] {
					xml.attribute("name",  Label(driver_and_port(), ".part"));
					xml.attribute("label", partition);
				});
			}
		});
	}
};

#endif /* _MODEL__STORAGE_TARGET_H_ */
