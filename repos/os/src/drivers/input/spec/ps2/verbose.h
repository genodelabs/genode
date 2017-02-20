/*
 * \brief  PS/2 driver
 * \author Norman Feske
 * \date   2017-01-02
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__VERBOSE_H_
#define _DRIVERS__INPUT__SPEC__PS2__VERBOSE_H_

#include <util/xml_node.h>

namespace Ps2 { struct Verbose; }


struct Ps2::Verbose
{
	bool const keyboard;
	bool const scancodes;
	bool const mouse;

	Verbose(Genode::Xml_node config)
	:
		keyboard (config.attribute_value("verbose_keyboard",  false)),
		scancodes(config.attribute_value("verbose_scancodes", false)),
		mouse    (config.attribute_value("verbose_mouse",     false))
	{ }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__VERBOSE_H_ */

