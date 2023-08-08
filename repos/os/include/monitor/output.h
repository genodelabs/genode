/*
 * \brief  Output utilities
 * \author Norman Feske
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR__OUTPUT_H_
#define _MONITOR__OUTPUT_H_

#include <base/output.h>

namespace Genode {

	struct Gdb_hex;
	struct Gdb_checksummed_output;
}


struct Genode::Gdb_hex : Hex
{
	template <typename T>
	explicit Gdb_hex(T value) : Hex(value, OMIT_PREFIX, PAD) { }
};


struct Genode::Gdb_checksummed_output : Output
{
	Output &_output;
	uint8_t _accumulated = 0;

	Gdb_checksummed_output(Output &output, bool notification) : _output(output)
	{
		if (notification)
			print(_output, "%");
		else
			print(_output, "$");
	}

	~Gdb_checksummed_output()
	{
		print(_output, "#", Gdb_hex(_accumulated));
	}

	void out_char(char c) override { out_string(&c, 1); }

	void out_string(char const *str, size_t n) override
	{
		n = (n == ~0UL) ? strlen(str) : n;

		for (unsigned i = 0; i < n; i++)
			_accumulated = uint8_t(_accumulated + str[i]);

		_output.out_string(str, n);
	}
};

#endif /* _MONITOR__OUTPUT_H_ */
