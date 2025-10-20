/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \date   2019-11-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_
#define _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_

/* Genode includes */
#include <cpu/page_flags.h>
#include <spec/arm/cpu/vcpu_state_trustzone.h>

/* core includes */
#include <hw/page_table.h>
#include <core_ram.h>
#include <types.h>

namespace Kernel { class Cpu; }
namespace Core { class Page_table_allocator; }

namespace Board {

	class Vcpu_state;

	enum { VCPU_MAX = 1 };

	struct Vm_page_table
	{
		Hw::Page_table_insertion_result
		insert(Genode::addr_t, Genode::addr_t, Genode::size_t,
		       Genode::Page_flags const &, Core::Page_table_allocator &) {
			return Genode::Ok(); }
		void remove(Genode::addr_t, Genode::size_t,
		            Core::Page_table_allocator &) { }
	};

	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}


class Board::Vcpu_state
{
	private:

		Genode::Vcpu_state *_state { nullptr };

	public:

		/* construction for this architecture always successful */
		using Error = Core::Accounted_mapped_ram_allocator::Error;
		using Constructed = Genode::Attempt<Genode::Ok, Error>;
		Constructed const constructed = Genode::Ok();

		/*
		 * Noncopyable
		 */
		Vcpu_state(Vcpu_state const &) = delete;
		Vcpu_state &operator = (Vcpu_state const &) = delete;

		Vcpu_state(Core::Accounted_mapped_ram_allocator &,
		           Core::Local_rm &,
		           Genode::Vcpu_state *state)
		: _state(state) {}

		void with_state(auto const fn) {
			if (_state != nullptr) fn(*_state); }
};


#endif /* _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_ */
