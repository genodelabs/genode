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
		Storage_device(env, alloc, label, capacity, sigh), model(model)
	{ }
};


struct Sculpt::Block_device_update_policy : List_model<Block_device>::Update_policy
{
	Env       &_env;
	Allocator &_alloc;

	Signal_context_capability _sigh;

	Block_device_update_policy(Env &env, Allocator &alloc, Signal_context_capability sigh)
	: _env(env), _alloc(alloc), _sigh(sigh) { }

	void destroy_element(Block_device &elem) { destroy(_alloc, &elem); }

	Block_device &create_element(Xml_node node)
	{
		return *new (_alloc)
			Block_device(_env, _alloc, _sigh,
			             node.attribute_value("label", Block_device::Label()),
			             node.attribute_value("model", Block_device::Model()),
			             Capacity { node.attribute_value("block_size",  0ULL)
			                      * node.attribute_value("block_count", 0ULL) });
	}

	void update_element(Block_device &, Xml_node) { }

	static bool element_matches_xml_node(Block_device const &elem, Xml_node node)
	{
		return node.attribute_value("label", Block_device::Label()) == elem.label;
	}
};

#endif /* _MODEL__BLOCK_DEVICE_H_ */
