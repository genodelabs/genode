/*
 * \brief   Virtual machine state
 * \author  Benjamin Lamowski
 * \date    2022-10-14
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__PC__VM_STATE_H_
#define _INCLUDE__SPEC__PC__VM_STATE_H_

#include <base/internal/page_size.h>
/* x86 CPU state */
#include <cpu/vcpu_state.h>

namespace Genode {

	/**
	 * CPU context of a virtual machine
	 */
	struct Vcpu_data
	{
		void *      virt_area;
		addr_t      phys_addr;
		Vcpu_state *vcpu_state;

		static constexpr size_t num_pages() {
			return 3;
		}

		static constexpr size_t size() {
			return get_page_size() * num_pages();
		}
	};
};

#endif /* _INCLUDE__SPEC__PC__VM_STATE_H_ */
