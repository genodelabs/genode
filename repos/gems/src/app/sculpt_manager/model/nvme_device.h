/*
 * \brief  Representation of NVMe devices
 * \author Norman Feske
 * \date   2024-04-22
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__NVME_DEVICE_H_
#define _MODEL__NVME_DEVICE_H_

#include <model/storage_device.h>

namespace Sculpt {

	struct Nvme_device;

	using Nvme_devices = List_model<Nvme_device>;
};


struct Sculpt::Nvme_device : List_model<Nvme_device>::Element, Storage_device
{
	using Model = String<16>;

	Model const model;

	static Port _port(Xml_node const &node)
	{
		return node.attribute_value("id", Port());
	}

	static Capacity _capacity(Xml_node const &node)
	{
		return { node.attribute_value("block_size",  0ULL)
		       * node.attribute_value("block_count", 0ULL) };
	}

	Nvme_device(Env &env, Allocator &alloc, Signal_context_capability sigh,
	            Model const &model, Xml_node const &node)
	:
		Storage_device(env, alloc, Storage_device::Provider::RUNTIME,
		               "nvme", _port(node), _capacity(node), sigh),
		model(model)
	{ }

	bool matches(Xml_node const &node) const { return _port(node) == port; }

	static bool type_matches(Xml_node const &) { return true; }
};

#endif /* _MODEL__NVME_DEVICE_H_ */
