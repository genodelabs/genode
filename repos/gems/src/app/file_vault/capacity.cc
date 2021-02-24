/*
 * \brief  Printable byte capacity
 * \author Norman Feske
 * \author Martin Stein
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <capacity.h>


/**************
 ** Capacity_**
 **************/

void File_vault::Capacity::print(Output &out) const
{
	static constexpr uint64_t KB = 1024;
	static constexpr uint64_t MB = 1024 * KB;
	static constexpr uint64_t GB = 1024 * MB;

	Text const text {
		(_value >= GB) ? Text((float)_value/GB, " GiB") :
		(_value >= MB) ? Text((float)_value/MB, " MiB") :
		(_value >= KB) ? Text((float)_value/KB, " KiB") :
		                 Text(_value,           " bytes") };

	Genode::print(out, text);
}


File_vault::Capacity::Capacity(uint64_t value)
:
	_value { value }
{ }


/*********************
 ** Capacity_string **
 *********************/

File_vault::Capacity_string::Capacity_string(uint64_t value)
:
	Capacity::Text { Capacity { value } }
{ }


File_vault::Capacity_string::operator char const *()
{
	return Capacity::Text::string();
}
