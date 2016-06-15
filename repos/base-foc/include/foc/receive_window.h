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

namespace Genode { struct Receive_window; }


class Genode::Receive_window
{
	private:

		/**
		 * Base of capability receive window.
		 */
		Native_capability::Data * _rcv_idx_base = nullptr;

		enum { MAX_CAPS_PER_MSG = Msgbuf_base::MAX_CAPS_PER_MSG };

	public:

		Receive_window() { }

		~Receive_window();

		void init();

		/**
		 * Return address of capability receive window
		 */
		addr_t rcv_cap_sel_base();

		/**
		 * Return received selector with index i
		 *
		 * \return   capability selector, or 0 if index is invalid
		 */
		addr_t rcv_cap_sel(unsigned i);
};

#endif /* _INCLUDE__FOC__RECEIVE_WINDOW_H_ */
