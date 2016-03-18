/*
 * \brief  Receive window for capability selectors
 * \author Norman Feske
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FOC__RECEIVE_WINDOW_H_
#define _INCLUDE__FOC__RECEIVE_WINDOW_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/ipc_msgbuf.h>
#include <base/cap_map.h>

namespace Genode { struct Receive_window; }


class Genode::Receive_window
{
	private:

		/**
		 * Base of capability receive window.
		 */
		Cap_index* _rcv_idx_base = nullptr;

		enum { MAX_CAPS_PER_MSG = Msgbuf_base::MAX_CAPS_PER_MSG };

	public:

		Receive_window() { }

		~Receive_window()
		{
			if (_rcv_idx_base)
				cap_idx_alloc()->free(_rcv_idx_base, MAX_CAPS_PER_MSG);
		}

		void init()
		{
			_rcv_idx_base = cap_idx_alloc()->alloc_range(MAX_CAPS_PER_MSG);
		}

		/**
		 * Return address of capability receive window
		 */
		addr_t rcv_cap_sel_base() { return _rcv_idx_base->kcap(); }

		/**
		 * Return received selector with index i
		 *
		 * \return   capability selector, or 0 if index is invalid
		 */
		addr_t rcv_cap_sel(unsigned i) {
			return rcv_cap_sel_base() + i*Fiasco::L4_CAP_SIZE; }
};

#endif /* _INCLUDE__FOC__RECEIVE_WINDOW_H_ */
