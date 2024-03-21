/*
 * \brief  Representation of AHCI and NVMe devices
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__BLOCK_DEVICE_H_
#define _MODEL__BLOCK_DEVICE_H_

#include "storage_device.h"

namespace Sculpt {

	struct Block_device;
	struct Block_device_update_policy;

	typedef List_model<Block_device> Block_devices;
};


struct Sculpt::Block_device : List_model<Block_device>::Element,
                              Storage_device
{
	typedef String<16> Model;

	Model const model;

	Block_device(Env &env, Allocator &alloc, Signal_context_capability sigh,
	             Label const &label, Model const &model, Capacity capacity)
	:
		Storage_device(env, alloc, Storage_device::Provider::PARENT,
		               label, Port { }, capacity, sigh), model(model)
	{ }

	bool matches(Xml_node const &node) const
	{
		return node.attribute_value("label", Block_device::Label()) == label;
	}

	static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _MODEL__BLOCK_DEVICE_H_ */
