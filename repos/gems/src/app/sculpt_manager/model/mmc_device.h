/*
 * \brief  Representation of MMC devices
 * \author Norman Feske
 * \date   2024-03-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__MMC_DEVICE_H_
#define _MODEL__MMC_DEVICE_H_

#include <model/storage_device.h>

namespace Sculpt {

	struct Mmc_device;

	using Mmc_devices = List_model<Mmc_device>;
};


struct Sculpt::Mmc_device : List_model<Mmc_device>::Element, Storage_device
{
	using Model = String<16>;

	Model const model;

	static Port _port(Xml_node const &node)
	{
		return node.attribute_value("label", Port());
	}

	static Capacity _capacity(Xml_node const &node)
	{
		return { node.attribute_value("block_size",  0ULL)
		       * node.attribute_value("block_count", 0ULL) };
	}

	Mmc_device(Env &env, Allocator &alloc, Xml_node const &node,
	           Storage_device::Action &action)
	:
		Storage_device(env, alloc, "mmc", _port(node), _capacity(node), action),
		model(node.attribute_value("model", Model()))
	{ }

	bool matches(Xml_node const &node) const { return _port(node) == port; }

	static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _MODEL__MMC_DEVICE_H_ */
