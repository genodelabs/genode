/*
 * \brief  Receive window for capability selectors
 * \author Alexander Boettcher
 * \author Norman Feske
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NOVA__RECEIVE_WINDOW_H_
#define _INCLUDE__NOVA__RECEIVE_WINDOW_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/ipc_msgbuf.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

namespace Genode { struct Receive_window; }


struct Genode::Receive_window
{
	public:

		enum {
			MAX_CAP_ARGS_LOG2 = 2,
			MAX_CAP_ARGS = 1UL << MAX_CAP_ARGS_LOG2
		};

		static_assert(MAX_CAP_ARGS == (size_t)Msgbuf_base::MAX_CAPS_PER_MSG,
		              "Inconsistency between Receive_window and Msgbuf_base");

	private:

		/**
		 * Base of portal receive window
		 */
		addr_t _rcv_pt_base = 0;

		struct {
			addr_t sel = 0;
			bool   del = 0;
		} _rcv_pt_sel[MAX_CAP_ARGS];

		/**
		 * Normally the received capabilities start from the beginning of
		 * the receive window (_rcv_pt_base), densely packed ascending.
		 * However, a receiver may send invalid caps, which will cause
		 * capability-selector gaps in the receiver window. Or a
		 * misbehaving sender may even intentionally place a cap at the end
		 * of the receive window. The position of a cap within the receive
		 * window is fundamentally important to correctly maintain the
		 * component-local capability-selector reference count.
		 *
		 * Additionally, the position is also required to decide whether a
		 * kernel capability must be revoked during the receive window
		 * cleanup/re-usage. '_rcv_pt_cap_free' is used to track this
		 * information in order to free up and revoke selectors
		 * (message-buffer cleanup).
		 *
		 * Meanings of the enums:
		 * - FREE_INVALID - invalid cap selector, no cap_map entry
		 * - FREE_SEL     - valid cap selector, invalid kernel capability
		 * - UNUSED_CAP   - valid selector and cap, not read/used yet
		 * - USED_CAP     - valid sel and cap, read/used by stream operator
		 */
		enum { FREE_INVALID, FREE_SEL, UNUSED_CAP, USED_CAP }
			_rcv_pt_cap_free [MAX_CAP_ARGS];

		/**
		 * Read counter for unmarshalling portal capability
		 * selectors
		 */
		unsigned short _rcv_pt_sel_cnt = 0;
		unsigned short _rcv_pt_sel_max = 0;
		unsigned short _rcv_wnd_log2 = 0;

		/**
		 * Reset portal-capability receive window
		 */
		void _rcv_reset()
		{
			if (!rcv_invalid()) { rcv_cleanup(false); }

			_rcv_pt_sel_cnt = 0;
			_rcv_pt_sel_max = 0;
			_rcv_pt_base = INVALID_INDEX;
		}

	public:

		enum { INVALID_INDEX = ~0UL };

		Receive_window()
		:
			_rcv_pt_base(INVALID_INDEX), _rcv_wnd_log2(MAX_CAP_ARGS_LOG2)
		{
			_rcv_reset();
		}

		~Receive_window()
		{
			_rcv_reset();
		}

		/**
		 * Set log2 number of capabilities to be received during reply of
		 * a IPC call.
		 */
		void rcv_wnd(unsigned short const caps_log2)
		{
			if (caps_log2 > MAX_CAP_ARGS_LOG2)
				nova_die();

			_rcv_wnd_log2 = caps_log2;
		}

		/**
		 * Return received portal-capability selector
		 */
		void rcv_pt_sel(Native_capability &cap);

		/**
		 * Return true if receive window must be re-initialized
		 */
		bool rcv_invalid() const;

		unsigned num_received_caps() const { return _rcv_pt_sel_max; }

		/**
		 * Return true if receive window must be re-initialized
		 *
		 * After reading portal selectors from the message
		 * buffer using 'rcv_pt_sel()', we assume that the IDC
		 * call populated the current receive window with one
		 * or more portal capabilities.
		 * To enable the reception of portal capability
		 * selectors for the next IDC, we need a fresh receive
		 * window.
		 *
		 * \param keep  'true' -  Try to keep receive window if
		 *                        it's clean.
		 *              'false' - Free caps of receive window
		 *                        because object is freed
		 *                        afterwards.
		 *
		 * \result 'true'  -  receive window must be re-initialized
		 *         'false' -  portal selectors has been kept
		 */
		bool rcv_cleanup(bool keep, unsigned short const new_max = MAX_CAP_ARGS);

		/**
		 * Initialize receive window for portal capability
		 * selectors
		 *
		 * \param utcb       - UTCB of designated receiver
		 *                     thread
		 * \param rcv_window - If specified - receive exactly
		 *                     one capability at the specified
		 *                     index of rcv_window
		 *
		 * Depending on the 'rcv_invalid', 'rcv_cleanup(true)'
		 * state of the message buffer and the specified
		 * rcv_window parameter, this function allocates a
		 * fresh receive window and clears 'rcv_invalid'.
		 */
		bool prepare_rcv_window(Nova::Utcb &utcb,
		                        addr_t rcv_window = INVALID_INDEX);

		/**
		 * Post IPC processing.
		 *
		 * Remember where and which caps have been received
		 * respectively have been translated.
		 * The information is required to correctly free
		 * cap indexes and to revoke unused received caps.
		 *
		 * \param utcb  UTCB of designated receiver thread
		 */
		void post_ipc(Nova::Utcb &utcb, addr_t const rcv_window = INVALID_INDEX)
		{
			using namespace Nova;

			unsigned const rcv_items = (utcb.items >> 16) & 0xffffu;

			_rcv_pt_sel_max = 0;
			_rcv_pt_sel_cnt = 0;

			unsigned short const max = 1U << utcb.crd_rcv.order();
			if (max > MAX_CAP_ARGS)
				nova_die();

			for (unsigned short i = 0; i < MAX_CAP_ARGS; i++) 
				_rcv_pt_cap_free [i] = (i >= max) ? FREE_INVALID : FREE_SEL;

			for (unsigned i = 0; i < rcv_items; i++) {
				Nova::Utcb::Item * item = utcb.get_item(i);
				if (!item)
					break;

				Nova::Crd cap(item->crd);

				/* track which items we got mapped */
				if (!cap.is_null() && item->is_del()) {
					/* should never happen */
					if (cap.base() < _rcv_pt_base ||
					    (cap.base() >= _rcv_pt_base + max))
						nova_die();
					_rcv_pt_cap_free [cap.base() - _rcv_pt_base] = UNUSED_CAP;
				}

				if (_rcv_pt_sel_max >= max) continue;

				/* track the order of mapped and translated items */
				if (cap.is_null()) {
					_rcv_pt_sel[_rcv_pt_sel_max].sel = INVALID_INDEX;
					_rcv_pt_sel[_rcv_pt_sel_max++].del = false;
				} else {
					_rcv_pt_sel[_rcv_pt_sel_max].sel = cap.base();
					_rcv_pt_sel[_rcv_pt_sel_max++].del = item->is_del();
				}
			}

			/*
			 * If a specific rcv_window has been specified,
			 * (see prepare_rcv_window) then the caller want to take care
			 * about freeing the * selector. Make the _rcv_pt_base invalid
			 * so that it is not cleanup twice.
			 */
			if (rcv_window != INVALID_INDEX)
				_rcv_pt_base = INVALID_INDEX;

			utcb.crd_rcv = 0;
		}
};

#endif /* _INCLUDE__NOVA__RECEIVE_WINDOW_H_ */
