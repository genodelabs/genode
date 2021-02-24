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

#ifndef _CAPACITY_H_
#define _CAPACITY_H_

/* local includes */
#include <types.h>

namespace File_vault {

	class Capacity;
	class Capacity_string;
}

class File_vault::Capacity
{
	private:

		uint64_t const _value;

	public:

		using Text = String<64>;

		Capacity(uint64_t value);

		void print(Output &out) const;
};

class File_vault::Capacity_string : public Capacity::Text
{
	public:

		Capacity_string(uint64_t value);

		operator char const *();
};

#endif /* _CAPACITY_H_ */
