/*
 * \brief  Finding and reporting an SMBIOS table as is (plain data)
 * \author Martin Stein
 * \date   2019-07-09
 */

 /*
  * Copyright (C) 2019 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

/* local includes */
#include <smbios_table_reporter.h>
#include <efi_system_table.h>

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <smbios/smbios.h>
#include <util/fifo.h>

using namespace Genode;

constexpr size_t get_page_size_log2() { return 12; }
constexpr size_t get_page_size()      { return 1 << get_page_size_log2(); }


Smbios_table_reporter::Smbios_table_reporter(Env       &env,
                                             Allocator &alloc)
{
	struct Io_region
	{
		Env                            &_env;
		addr_t                          _base_page;
		size_t                          _size_pages;
		Fifo<Fifo_element<Io_region> > &_fifo;
		Attached_io_mem_dataspace       _io_mem    { _env, _base_page, _size_pages };
		Fifo_element<Io_region>         _fifo_elem { *this };

		Io_region(Env                            &env,
		          addr_t                          base_page,
		          size_t                          size_pages,
		          Fifo<Fifo_element<Io_region> > &fifo)
		:
			_env        { env },
			_base_page  { base_page },
			_size_pages { size_pages },
			_fifo       { fifo }
		{
			_fifo.enqueue(_fifo_elem);
		}

		~Io_region()
		{
			_fifo.remove(_fifo_elem);
		}
	};

	Fifo<Fifo_element<Io_region> > io_regions;
	addr_t const page_mask     { ~(addr_t)((1 << get_page_size_log2()) - 1) };
	addr_t const page_off_mask { get_page_size() - 1 };
	auto phy_mem = [&] (addr_t base, size_t size) {

		addr_t const  end      { base + size };
		Io_region    *reuse_io { nullptr };
		io_regions.for_each([&] (Fifo_element<Io_region> &elem) {
			Io_region &io { elem.object() };
			if (io._base_page <= base &&
			    io._base_page + io._size_pages >= end) {

				reuse_io = &io;
			}
		});
		if (reuse_io) {
			addr_t const off { base - reuse_io->_base_page };
			return (addr_t)reuse_io->_io_mem.local_addr<int>() + off;
		}
		addr_t const base_page { base & page_mask };
		addr_t const base_off  { base - base_page };
		size += base_off;
		size_t const size_pages { (size + page_off_mask) & page_mask };
		addr_t       alloc_base { base_page };
		addr_t       alloc_end  { base_page + size_pages };

		io_regions.for_each([&] (Fifo_element<Io_region> &elem) {

			Io_region    &io { elem.object() };
			addr_t const  io_base { io._base_page };
			addr_t const  io_end  { io._base_page + io._size_pages };
			bool          io_destroy { false };

			if (io_base < alloc_base && io_end > alloc_base) {
				alloc_base = io_base;
				io_destroy = true;
			}
			if (io_base < alloc_end && io_end > alloc_end) {
				alloc_end = io_end;
				io_destroy = true;
			}
			if (io_base >= alloc_base && io_end <= alloc_end) {
				io_destroy = true;
			}
			if (io_destroy) {
				destroy(&alloc, &io);
			}
		});
		size_t alloc_size { alloc_end - alloc_base };
		Io_region *io {
			new (alloc) Io_region(env, alloc_base, alloc_size, io_regions) };

		addr_t const off { base - io->_base_page };
		return (addr_t)io->_io_mem.local_addr<int>() + off;
	};
	auto report_smbios = [&] (void  *ep_vir, size_t ep_size,
	                          addr_t st_phy, size_t st_size)
	{
		addr_t const st_vir   { phy_mem(st_phy, st_size) };
		size_t const ram_size { ep_size + st_size };
		addr_t const ram_vir  { (addr_t)alloc.alloc(ram_size) };

		memcpy((void *)ram_vir, ep_vir, ep_size);
		memcpy((void *)(ram_vir + ep_size), (void *)st_vir, st_size);

		_reporter.construct(env, "smbios_table", "smbios_table", ram_size);
		_reporter->enabled(true);
		_reporter->report((void *)ram_vir, ram_size);

		alloc.free((void *)ram_vir, ep_size + st_size);
	};
	auto handle_smbios_3 = [&] (Smbios_3_entry_point const &ep)
	{
		report_smbios((void *)&ep, ep.length, ep.struct_table_addr,
		              ep.struct_table_max_size);
	};
	auto handle_smbios = [&] (Smbios_entry_point const &ep)
	{
		report_smbios((void *)&ep, ep.length, ep.struct_table_addr,
		              ep.struct_table_length);
	};
	auto handle_dmi = [&] (Dmi_entry_point const &ep)
	{
		report_smbios((void *)&ep, ep.LENGTH, ep.struct_table_addr,
		              ep.struct_table_length);
	};

	addr_t efi_sys_tab_phy = 0;
	try {
		Attached_rom_dataspace info(env, "platform_info");
		Xml_node xml(info.local_addr<char>(), info.size());
		Xml_node acpi_node = xml.sub_node("efi-system-table");
		efi_sys_tab_phy = acpi_node.attribute_value("address", 0UL);
	} catch (...) { }

	if (!efi_sys_tab_phy) {
		Smbios_table::from_scan(phy_mem, handle_smbios_3,
		                        handle_smbios, handle_dmi);
	} else {
		Efi_system_table const &efi_sys_tab_vir { *(Efi_system_table *)
			phy_mem(efi_sys_tab_phy, sizeof(Efi_system_table)) };

		efi_sys_tab_vir.for_smbios_table(phy_mem, [&] (addr_t table_phy) {
			Smbios_table::from_pointer(table_phy, phy_mem, handle_smbios_3,
			                           handle_smbios, handle_dmi);
		});
	}
	io_regions.for_each([&] (Fifo_element<Io_region> &elem) {
		destroy(alloc, &elem.object());
	});
}
