/*
 * \brief  Textual output functions
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/output.h>

/* base-internal includes */
#include <base/internal/output.h>

using namespace Genode;


/************
 ** Output **
 ************/

void Output::out_string(char const *str, size_t n)
{
	if (!str)
		return;

	while (*str && n--) out_char(*str++);
}


/******************************
 ** Print function overloads **
 ******************************/

void Genode::print(Output &output, char const *str)
{
	if (!str)
		output.out_string("<null-string>");
	else
		while (*str) output.out_char(*str++);
}


void Genode::print(Output &output, void const *ptr)
{
	print(output, Hex(reinterpret_cast<addr_t>(ptr)));
}


void Genode::print(Output &output, unsigned long value)
{
	out_unsigned<unsigned long>(value, 10, 0, [&] (char c) { output.out_char(c); });
}


void Genode::print(Output &output, unsigned long long value)
{
	out_unsigned<unsigned long long>(value, 10, 0, [&] (char c) { output.out_char(c); });
}


void Genode::print(Output &output, long value)
{
	out_signed<long>(value, 10, [&] (char c) { output.out_char(c); });
}


void Genode::print(Output &output, long long value)
{
	out_signed<long long>(value, 10, [&] (char c) { output.out_char(c); });
}

void Genode::print(Output &output, float value)
{
	out_float<float>(value, 10, 3, [&] (char c) { output.out_char(c); });
}

void Genode::print(Output &output, double value)
{
	out_float<double>(value, 10, 6, [&] (char c) { output.out_char(c); });
}

void Genode::Hex::print(Output &output) const
{
	if (_prefix == Hex::PREFIX)
		output.out_string("0x");

	size_t const pad_len = (_pad == Hex::PAD) ? _digits : 0;

	/* mask possible sign-extension bits */
	unsigned long long mask = 0;
	for (size_t i = 0; i < _digits; ++i) mask = (mask << 4) | 0xf;

	out_unsigned<unsigned long long>(_value & mask, 16, pad_len,
	                                [&] (char c) { output.out_char(c); });
}

