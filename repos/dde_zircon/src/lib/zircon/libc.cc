/*
 * \brief  Zircon libc definitions
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <timer_session/connection.h>
#include <util/string.h>

#include <zx/static_resource.h>


class Format_String
{
	private:
		static char _buffer[1024];

	public:
		Format_String(const char *prefix, const char *str, Genode::size_t len)
		{
			Genode::size_t last = Genode::strlen(_buffer);
			Genode::memcpy(_buffer + last, str, Genode::min(len, 1023 - last));
			for (Genode::size_t i = 0; i < Genode::min(len, 1023 - last); i++){
				_buffer[i + last] = str[i];
				if (_buffer[i + last] == '\n'){
					Genode::log(Genode::Cstring(prefix), " ", *this);
					Genode::memset(_buffer, '\0', 1024);
					last = 0;
				}
			}
		}

		void print(Genode::Output &out) const
		{
			_buffer[Genode::strlen(_buffer) - 1] = '\0';
			Genode::print(out, Genode::Cstring(_buffer));
		}
};

char Format_String::_buffer[1024] = {};

extern "C" {

	int usleep(unsigned usecs)
	{
		ZX::Resource<Timer::Connection>::get_component().usleep(usecs);
		return 0;
	}

	int __printf_output_func(const char *str, Genode::size_t len, void *)
	{
		Format_String fmt("ZIRCON:", str, len);
		return 0;
	}

	Genode::size_t strlen(char *str)
	{
		Genode::size_t len = 0;
		while (str[++len] != '\0');
		return len;
	}

	void *malloc(Genode::size_t size)
	{
		void *mem;
		Genode::Heap &heap = ZX::Resource<Genode::Heap>::get_component();
		if (!heap.alloc(size, &mem)){
			return nullptr;
		}
		return mem;
	}

	void free(void *ptr)
	{
		Genode::Heap &heap = ZX::Resource<Genode::Heap>::get_component();
		heap.free(ptr, 0);
	}

	void *calloc(Genode::size_t elem, Genode::size_t size)
	{
		return malloc(elem * size);
	}

}
