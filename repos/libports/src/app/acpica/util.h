/*
 * \brief  Some utils
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2016-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {
#include "acpi.h"
}

class Bridge;

namespace Acpica {
	template<typename> class Buffer;
	template<typename> class Callback;
	void generate_report(Genode::Env &, Bridge *);
}

template <typename T>
class Acpica::Buffer : public ACPI_BUFFER
{
	public:
		T object;
		Buffer() {
			Pointer = &object;
			Length = sizeof(object);
		}

} __attribute__((packed));

template <typename T>
class Acpica::Callback : public Genode::List<Acpica::Callback<T> >::Element
{
	public:

		static void handler(ACPI_HANDLE h, UINT32 value, void * context)
		{
			reinterpret_cast<T *>(context)->handle(h, value);
		}

		void generate(Genode::Xml_generator &xml) {
			reinterpret_cast<T *>(this)->generate(xml);
		}
};
