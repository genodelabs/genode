/*
 * \brief  Identifiers for the Tresor modules used in the Tresor tester
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODULE_TYPE_H_
#define _MODULE_TYPE_H_

/* Genode includes */
#include <base/stdint.h>

enum class Module_type : Genode::uint8_t
{
	TRESOR_INIT,
	TRESOR_CHECK,
	TRESOR,
};


static Module_type module_type_from_uint32(Genode::uint32_t uint32)
{
	class Bad_tag { };
	switch (uint32) {
	case 1: return Module_type::TRESOR_INIT;
	case 2: return Module_type::TRESOR;
	case 4: return Module_type::TRESOR_CHECK;
	default: throw Bad_tag();
	}
}


static Genode::uint32_t module_type_to_uint32(Module_type type)
{
	class Bad_type { };
	switch (type) {
	case Module_type::TRESOR_INIT : return 1;
	case Module_type::TRESOR      : return 2;
	case Module_type::TRESOR_CHECK: return 4;
	}
	throw Bad_type();
}


static Module_type tag_get_module_type(Genode::uint32_t tag)
{
	return module_type_from_uint32((tag >> 24) & 0xff);
}


static Genode::uint32_t tag_set_module_type(Genode::uint32_t    tag,
                                    Module_type type)
{
	if (tag >> 24) {

		class Bad_tag { };
		throw Bad_tag();
	}
	return tag | (module_type_to_uint32(type) << 24);
}


static Genode::uint32_t tag_unset_module_type(Genode::uint32_t tag)
{
	return tag & 0xffffff;
}

#endif /* _MODULE_TYPE_H_ */
