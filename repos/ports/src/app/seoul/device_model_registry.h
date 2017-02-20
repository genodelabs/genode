/*
 * \brief  Meta-data registry about the device models of Vancouver
 * \author Norman Feske
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DEVICE_MODEL_REGISTRY_H_
#define _DEVICE_MODEL_REGISTRY_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>

/* NOVA userland includes */
#include <nul/motherboard.h>

struct Device_model_info : Genode::List<Device_model_info>::Element
{
	typedef void (*Create)(Motherboard &, unsigned long *argv,
	                       const char *args, unsigned args_len);

	/**
	 * Name of device model
	 */
	char const *name;

	/**
	 * Function for creating a new device-model instance
	 */
	Create create;

	/**
	 * Argument names
	 */
	char const **arg_names;

	Device_model_info(char const *name, Create create, char const *arg_names[]);
};


struct Device_model_registry : Genode::List<Device_model_info>
{
	Device_model_info *lookup(char const *name)
	{
		for (Device_model_info *curr = first(); curr; curr = curr->next())
			if (Genode::strcmp(curr->name, name) == 0)
				return curr;

		return 0;
	}
};


/**
 * Return singleton instance of device model registry
 */
Device_model_registry *device_model_registry();

#endif /* _DEVICE_MODEL_REGISTRY_H_ */
