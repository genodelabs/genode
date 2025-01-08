/*
 * \brief  Platform interface
 * \author Stefan Kalkowski
 * \date   2016-10-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__PLATFORM_H_
#define _SRC__BOOTSTRAP__PLATFORM_H_

#include <base/allocator_avl.h>
#include <base/internal/elf.h>
#include <hw/boot_info.h>
#include <util/reconstructible.h>

#include <board.h>

namespace Bootstrap {

	struct Platform;

	using Genode::addr_t;
	using Genode::size_t;
	using Genode::uint32_t;
	using Boot_info = Hw::Boot_info<::Board::Boot_info>;
	using Hw::Mmio_space;
	using Hw::Mapping;
	using Hw::Memory_region;
	using Hw::Memory_region_array;

	extern Platform & platform();
}


class Bootstrap::Platform
{
	private:

		struct Board
		{
			Memory_region_array early_ram_regions { };
			Memory_region_array late_ram_regions  { };
			Mmio_space          core_mmio;
			unsigned            cpus { };
			::Board::Boot_info  info { };

			Board();
		};


		class Ram_allocator : private Genode::Allocator_avl_base
		{
			/*
			 * 'Ram_allocator' is derived from 'Allocator_avl_base' to access
			 * the protected 'slab_block_size'.
			 */

			private:

				using Base = Genode::Allocator_avl_base;

				enum { BSZ = Genode::Allocator_avl::slab_block_size() };

				Genode::Tslab<Base::Block, BSZ> _slab;
				Genode::uint8_t                 _first_slab[BSZ];

			public:

				Ram_allocator()
				:
					Genode::Allocator_avl_base(&_slab, sizeof(Base::Block)),
					_slab(this, (Block *)&_first_slab)
				{ }

				void * alloc_aligned(size_t size, unsigned align);
				void * alloc(size_t size) { return alloc_aligned(size, 0); }

				void add(Memory_region const &);
				void remove(Memory_region const &);

				void for_each_free_region(auto const &fn)
				{
					_block_tree().for_each([&] (Block const & b)
					{
						if (!b.used())
							fn(Memory_region(b.addr(), b.size()));
					});
				}
		};


		struct Pd
		{
			using Table = Hw::Page_table;
			using Table_array = Table::Allocator::Array<Table::CORE_TRANS_TABLE_COUNT>;

			void * const             table_base;
			void * const             array_base;
			Table                  & table;
			Table_array            & array;
			Boot_info::Mapping_pool  mappings { };

			Pd(Ram_allocator & alloc);

			void map(Mapping m);
			void map_insert(Mapping m);
		};


		struct Elf : Genode::Elf_binary
		{
			Elf(addr_t const addr) : Genode::Elf_binary(addr) { }

			void for_each_segment(auto const &fn)
			{
				Genode::Elf_segment segment;
				for (unsigned i = 0; (segment = get_segment(i)).valid(); i++) {
					if (segment.flags().skip)    continue;
					if (segment.mem_size() == 0) continue;
					fn(segment);
				}
			}
		};

		Board                     board     { };
		Ram_allocator             ram_alloc { };
		Memory_region const       bootstrap_region;
		Genode::Constructible<Pd> core_pd { };
		addr_t                    core_elf_addr;
		Elf                       core_elf;

		Mapping  _load_elf();
		void     _prepare_cpu_memory_area(size_t id);
		unsigned _prepare_cpu_memory_area();

	public:

		Platform();

		unsigned enable_mmu();
		void start_core(unsigned) __attribute__((noreturn));
};

#endif /* _SRC__BOOTSTRAP__PLATFORM_H_ */
