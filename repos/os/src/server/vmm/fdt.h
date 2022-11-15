/*
 * \brief  VMM utilities to generate a flattened device tree blob
 * \author Stefan Kalkowski
 * \date   2022-11-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__FDT_H_
#define _SRC__SERVER__VMM__FDT_H_

#include <config.h>
#include <base/env.h>
#include <base/heap.h>
#include <util/dictionary.h>

namespace Vmm {
	class Fdt_generator;

	using namespace Genode;
}


class Vmm::Fdt_generator
{
	private:

		class Fdt_dictionary
		{
			public:

				struct Name : Genode::String<64>
			{
				using String<64>::String;

				template <typename T>
					void write(uint32_t off, T & buf) const {
						buf.write(off, string(), length()); }
			};

			private:

				struct String : Dictionary<String, Name>::Element
				{
					uint32_t const offset;

					String(Dictionary<String, Name> & dict,
					       Name const               & name,
					       uint32_t                   offset)
					:
						Dictionary<String, Name>::Element(dict, name),
						offset(offset) {}
				};

				Heap                   & _heap;
				uint32_t                 _offset { 0U };
				Dictionary<String, Name> _dict {};

			public:

				struct Not_found {};

				Fdt_dictionary(Heap & heap) : _heap(heap) { }

				~Fdt_dictionary()
				{
					for (;;)
						if (!_dict.with_any_element([&] (String & str) {
							destroy(_heap, &str); }))
							break;
				}

				void add(Name const & name)
				{
					_dict.with_element(name, [&] (String const &) {}, [&] ()
					{
						new (_heap) String(_dict, name, _offset);
						_offset += (uint32_t)name.length();
					});
				}

				uint32_t offset(Name const & name) const
				{
					return _dict.with_element(name, [&] (String const & str)
					{
						return str.offset;
					}, [&] ()
					{
						throw Not_found();
						return 0;
					});
				}

				template <typename WRITE_FN>
				void write(WRITE_FN const & write_fn) const
				{
					_dict.for_each([&] (String const & str)
					{
						write_fn(str.offset, str.name.string(), str.name.length());
					});
				}

				uint32_t length() const { return _offset; }
		};


		struct Buffer
		{
			struct Buffer_exceeded {};

			addr_t const addr;
			size_t const size;
			size_t       used { 0 };

			Buffer(addr_t addr, size_t size) : addr(addr), size(size) {}

			void write(addr_t offset, void const * data, size_t length)
			{
				if ((offset + length) > size)
					throw Buffer_exceeded();

				memcpy((void*)(addr + offset), data, length);
				if ((offset + length) > used) used = offset + length;
			}
		};

		Env          & _env;
		Heap         & _heap;
		Buffer         _buffer;
		Fdt_dictionary _dict { _heap };

		void _generate_tree(uint32_t & off, Config const & config,
                            void * initrd_start, size_t initrd_size);

	public:

		Fdt_generator(Env & env, Heap & heap, addr_t dtb_addr, size_t max_size)
		: _env(env), _heap(heap), _buffer(dtb_addr, max_size) { }

		void generate(Config const & config, void * initrd_start,
		              size_t initrd_size);
};

#endif /* _SRC__SERVER__VMM__FDT_H_ */
