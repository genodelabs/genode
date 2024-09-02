/*
 * \brief  Common types used within the window manager
 * \author Norman Feske
 * \date   2024-09-04
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <base/tslab.h>
#include <base/attached_rom_dataspace.h>

namespace Wm {

	using Genode::uint8_t;
	using Genode::size_t;
	using Genode::Rom_connection;
	using Genode::Xml_node;
	using Genode::Attached_rom_dataspace;
	using Genode::Tslab;
	using Genode::Cap_quota;
	using Genode::Ram_quota;

	/*
	 * Slab allocator that includes an initial block as member
	 */
	template <size_t BLOCK_SIZE>
	struct Initial_slab_block { uint8_t buf[BLOCK_SIZE]; };
	template <typename T, size_t BLOCK_SIZE>
	struct Slab : private Initial_slab_block<BLOCK_SIZE>, Tslab<T, BLOCK_SIZE>
	{
		Slab(Allocator &block_alloc)
		: Tslab<T, BLOCK_SIZE>(block_alloc, Initial_slab_block<BLOCK_SIZE>::buf) { };
	};

	struct Upgradeable : Genode::Noncopyable
	{
		bool _starved_for_ram = false, _starved_for_caps = false;

		void _upgrade_local_or_remote(auto const &resources, auto &session_obj, auto &real_gui)
		{
			Ram_quota ram  = resources.ram_quota;
			Cap_quota caps = resources.cap_quota;

			if (_starved_for_caps && caps.value) {
				session_obj.upgrade(caps);
				_starved_for_caps = false;
				caps = { };
			}

			if (_starved_for_ram && ram.value) {
				session_obj.upgrade(ram);
				_starved_for_ram = false;
				ram = { };
			}

			if (ram.value || caps.value) {
				real_gui.connection.upgrade({ ram, caps });
			}
		}
	};
}

#endif /* _TYPES_H_ */
