/*
 * \brief  Option fields
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__OPTION_H_
#define _PCAPNG__OPTION_H_

#include <trace_recorder_policy/pcapng.h>

namespace Pcapng {
	using namespace Genode;

	template <uint16_t TYPE>
	struct Option;

	struct Option_ifname;
	struct Option_end;
}

template <Genode::uint16_t TYPE>
struct Pcapng::Option
{
	uint16_t _type   { TYPE };
	uint16_t _length;
	uint32_t _data[0];

	static uint16_t padded_size(uint16_t sz) {
		return (uint16_t)align_addr((uint32_t)sz, 2); }

	Option(uint16_t length)
	: _length(length)
	{ }

	template <typename T>
	T *data() { return (T*)_data; }

	uint16_t total_length() const { return padded_size(sizeof(Option) + _length); }
} __attribute__((packed));


struct Pcapng::Option_end : Option<1>
{
	Option_end() : Option(0) { }
} __attribute__((packed));


struct Pcapng::Option_ifname : Option<2>
{
	static uint16_t padded_size(Interface_name const &name) {
		return (uint16_t)align_addr((uint32_t)name.data_length() - 1, 2); }

	Option_ifname(Interface_name const &name)
	: Option((uint16_t)(name.data_length()-1))
	{
		/* copy string leaving out null-termination */
		memcpy(data<void>(), name.string(), name.data_length()-1);
	}
} __attribute__((packed));

#endif /* _PCAPNG__OPTION_H_ */
