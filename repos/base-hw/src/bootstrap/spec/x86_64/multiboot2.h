/*
 * \brief  Multiboot 2 handling
 * \author Alexander Boettcher
 * \date   2017-08-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_
#define _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_

/* base includes */
#include <util/mmio.h>

namespace Genode { class Multiboot2_info; }

class Genode::Multiboot2_info : Mmio<0x8>
{
	private:

		struct Size : Register <0x0, 32> { };

		template <size_t SIZE>
		struct Tag_tpl : Genode::Mmio<SIZE>
		{
			enum { LOG2_SIZE = 3 };

			struct Type : Register <0x00, 32>
			{
				enum {
					END                 = 0,
					MEMORY              = 6,
					FRAMEBUFFER         = 8,
					EFI_SYSTEM_TABLE_64 = 12,
					ACPI_RSDP_V1        = 14,
					ACPI_RSDP_V2        = 15,
				};
			};
			struct Size : Register <0x04, 32> { };

			Tag_tpl(addr_t addr) : Mmio<SIZE>({(char *)addr, SIZE}) { }
		};

		using Tag = Tag_tpl<0x8>;

		struct Efi_system_table_64 : Tag_tpl<0x10>
		{
			struct Pointer : Register <0x08, 64> { };

			Efi_system_table_64(addr_t addr) : Tag_tpl(addr) { }
		};

	public:

		enum { MAGIC = 0x36d76289UL };

		struct Memory : Genode::Mmio<0x14>
		{
			enum { SIZE = 3 * 8 };

			struct Addr : Register <0x00, 64> { };
			struct Size : Register <0x08, 64> { };
			struct Type : Register <0x10, 32> { enum { MEMORY = 1 }; };

			Memory(addr_t mmap = 0) : Mmio({(char *)mmap, Mmio::SIZE}) { }
		};

		Multiboot2_info(addr_t mbi) : Mmio({(char *)mbi, Mmio::SIZE}) { }

		void for_each_tag(auto const &mem_fn,
		                  auto const &acpi_rsdp_v1_fn,
		                  auto const &acpi_rsdp_v2_fn,
		                  auto const &fb_fn,
		                  auto const &systab64_fn)
		{
			addr_t const size = read<Multiboot2_info::Size>();

			for (addr_t tag_addr = base() + (1UL << Tag::LOG2_SIZE);
			     tag_addr < base() + size;)
			{
				Tag tag(tag_addr);

				if (tag.read<Tag::Type>() == Tag::Type::END)
					return;

				if (tag.read<Tag::Type>() == Tag::Type::EFI_SYSTEM_TABLE_64) {
					Efi_system_table_64 const est(tag_addr);
					systab64_fn(est.read<Efi_system_table_64::Pointer>());
				}
				if (tag.read<Tag::Type>() == Tag::Type::MEMORY) {
					addr_t mem_start = tag_addr + (1UL << Tag::LOG2_SIZE) + 8;
					addr_t const mem_end = tag_addr + tag.read<Tag::Size>();

					for (; mem_start < mem_end; mem_start += Memory::SIZE) {
						Memory mem(mem_start);
						mem_fn(mem);
					}
				}

				if (tag.read<Tag::Type>() == Tag::Type::ACPI_RSDP_V1 ||
				    tag.read<Tag::Type>() == Tag::Type::ACPI_RSDP_V2) {

					size_t const sizeof_tag = 1UL << Tag::LOG2_SIZE;
					addr_t const rsdp_addr  = tag_addr + sizeof_tag;

					Hw::Acpi_rsdp * rsdp = reinterpret_cast<Hw::Acpi_rsdp *>(rsdp_addr);

					if (tag.read<Tag::Size>() - sizeof_tag == 20) {
						/* ACPI RSDP v1 is 20 byte solely */
						Hw::Acpi_rsdp rsdp_v1;
						memset (&rsdp_v1, 0, sizeof(rsdp_v1));
						memcpy (&rsdp_v1, rsdp, 20);
						acpi_rsdp_v1_fn(rsdp_v1);
					} else
					if (sizeof(*rsdp) <= tag.read<Tag::Size>() - sizeof_tag) {
						/* ACPI RSDP v2 */
						acpi_rsdp_v2_fn(*rsdp);
					}
				}

				if (tag.read<Tag::Type>() == Tag::Type::FRAMEBUFFER) {
					size_t const sizeof_tag = 1UL << Tag::LOG2_SIZE;
					addr_t const fb_addr  = tag_addr + sizeof_tag;

					Hw::Framebuffer const * fb = reinterpret_cast<Hw::Framebuffer *>(fb_addr);
					if (sizeof(*fb) <= tag.read<Tag::Size>() - sizeof_tag)
						fb_fn(*fb);
				}

				tag_addr += align_addr(tag.read<Tag::Size>(), Tag::LOG2_SIZE);
			}
		}
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT2_H_ */
