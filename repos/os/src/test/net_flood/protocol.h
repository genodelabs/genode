/*
 * \brief  Supported protocols
 * \author Martin Stein
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { enum class Protocol : uint16_t { ICMP, UDP, TCP }; }

namespace Genode
{
	inline size_t ascii_to(char const *s, Protocol &result)
	{
		if (!strcmp(s, "icmp", 4)) { result = Protocol::ICMP; return 4; }
		if (!strcmp(s, "udp",  3)) { result = Protocol::UDP;  return 3; }
		if (!strcmp(s, "tcp",  3)) { result = Protocol::TCP;  return 3; }
		return 0;
	}
}

#endif /* __PROTOCOL_H_ */
