/*
 * \brief  Provide protocol names as Genode Cstring objects
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROTOCOL_NAME_H_
#define _PROTOCOL_NAME_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { class Cstring; }

namespace Net {

	Genode::Cstring const &tcp_name();
	Genode::Cstring const &udp_name();
	Genode::Cstring const &protocol_name(Genode::uint8_t protocol);
}

#endif /* _PROTOCOL_NAME_H_ */
