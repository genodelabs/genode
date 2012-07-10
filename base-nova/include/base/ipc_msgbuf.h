/*
 * \brief  IPC message buffer layout for NOVA
 * \author Norman Feske
 * \date   2009-10-02
 *
 * On NOVA, we use IPC to transmit plain data and for capability delegation.
 * Therefore the message buffer contains both categories of payload. The
 * capability-specific part are the members '_snd_pt*' (sending capability
 * selectors) and '_rcv_pt*' (receiving capability selectors).
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

/* Genode includes */
#include <base/cap_sel_alloc.h>

/* NOVA includes */
#include <nova/syscalls.h>

namespace Genode {

	class Msgbuf_base
	{
		public:

			enum { MAX_CAP_ARGS_LOG2 = 2, MAX_CAP_ARGS = 1 << MAX_CAP_ARGS_LOG2 };

		protected:

			size_t _size;

			/**
			 * Number of portal-capability selectors to send
			 */
			size_t _snd_pt_sel_cnt;

			/**
			 * Portal capability selectors to delegate
			 */
			addr_t _snd_pt_sel[MAX_CAP_ARGS];

			/**
			 * Base of portal receive window
			 */
			int _rcv_pt_base;

			struct {
				addr_t sel;
				bool   del;
			} _rcv_pt_sel [MAX_CAP_ARGS];

			/**
			 * Read counter for unmarshalling portal capability selectors
			 */
			unsigned _rcv_pt_sel_cnt;
			unsigned _rcv_pt_sel_max;

			/**
			 * Flag set to true if receive window must be re-initialized
			 */
			bool _rcv_dirty;

			char _msg_start[];  /* symbol marks start of message */

		public:

			/**
			 * Constructor
			 */
			Msgbuf_base() : _rcv_dirty(true)
			{
				rcv_reset();
				snd_reset();
			}

			/*
			 * Begin of actual message buffer
			 */
			char buf[];

			/**
			 * Return size of message buffer
			 */
			inline size_t size() const { return _size; }

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &_msg_start[0]; }

			/**
			 * Reset portal capability selector payload
			 */
			inline void snd_reset() { _snd_pt_sel_cnt = 0; }

			/**
			 * Append portal capability selector to message buffer
			 */
			inline bool snd_append_pt_sel(addr_t pt_sel)
			{
				if (_snd_pt_sel_cnt >= MAX_CAP_ARGS - 1)
					return false;

				_snd_pt_sel[_snd_pt_sel_cnt++] = pt_sel;
				return true;
			}

			/**
			 * Return number of marshalled portal-capability selectors
			 */
			inline size_t snd_pt_sel_cnt() { return _snd_pt_sel_cnt; }

			/**
			 * Return portal capability selector
			 *
			 * \param i  index (0 ... 'pt_sel_cnt()' - 1)
			 * \return   portal-capability selector, or
			 *           -1 if index is invalid
			 */
			addr_t snd_pt_sel(unsigned i) { return i < _snd_pt_sel_cnt ? _snd_pt_sel[i] : -1; }

			/**
			 * Request current portal-receive window
			 */
			addr_t rcv_pt_base() { return _rcv_pt_base; }

			/**
			 * Reset portal-capability receive window
			 */
			void rcv_reset()
			{
				if (!rcv_invalid()) { rcv_cleanup(false); }

				_rcv_pt_sel_cnt = 0;
				_rcv_pt_sel_max = 0;
				_rcv_pt_base = ~0UL;
			}

			/**
			 * Return received portal-capability selector
			 */
			addr_t rcv_pt_sel()
			{
				/* return only received or translated caps */
				if (_rcv_pt_sel_cnt < _rcv_pt_sel_max)
					return _rcv_pt_sel[_rcv_pt_sel_cnt++].sel;
				else
					return 0;
			}

			/**
			 * Return true if receive window must be re-initialized
			 *
			 * After reading portal selectors from the message buffer using
			 * 'rcv_pt_sel()', we assume that the IDC call populared the
			 * current receive window with one or more portal capabilities.
			 * To enable the reception of portal capability selectors for the
			 * next IDC, we need a fresh receive window.
			 *
			 * \param keep  If 'true', try to keep receive window if it's clean.
			 *              If 'false', free caps of receive window because object is freed afterwards.
			 *
			 * \result 'true' if receive window was freed, 'false' if it was kept.
			 */
			bool rcv_cleanup(bool keep)
			{
				/* If nothing has been used, revoke/free at once */
				if (_rcv_pt_sel_cnt == 0) {
					_rcv_pt_sel_max = 0;

					if (_rcv_items)
						Nova::revoke(Nova::Obj_crd(rcv_pt_base(), MAX_CAP_ARGS_LOG2), true);

					if (keep) return false;

					cap_selector_allocator()->free(rcv_pt_base(), MAX_CAP_ARGS_LOG2);
					return true;
				}

				/* Revoke received unused caps, skip translated caps */
				for (unsigned i = _rcv_pt_sel_cnt; i < _rcv_pt_sel_max; i++) {
					if (_rcv_pt_sel[i].del) {
						/* revoke cap we received but didn't use */
						Nova::revoke(Nova::Obj_crd(_rcv_pt_sel[i].sel, 0), true);
						/* mark cap as free */
						cap_selector_allocator()->free(_rcv_pt_sel[i].sel, 0);
					}
				}

				/* free all caps which are unused from the receive window */
				for (unsigned i=0; i < MAX_CAP_ARGS; i++) {
					unsigned j=0;
					for (j=0; j < _rcv_pt_sel_max; j++) {
						if (_rcv_pt_sel[j].sel == rcv_pt_base() + i) break;
					}
					if (j < _rcv_pt_sel_max) continue;

					/* Revoke seems needless here, but is required.
					 * It is possible that an evil guy translated us
					 * more then MAX_CAP_ARGS and then mapped us
					 * something to the receive window.
					 * The mappings would not show up
					 * in _rcv_pt_sel, but would be there.
					 */
					Nova::revoke(Nova::Obj_crd(rcv_pt_base() + i, 0), true);
					/* i was unused, free at allocator */ 
					cap_selector_allocator()->free(rcv_pt_base() + i, 0);
				}

				_rcv_pt_sel_cnt = 0;
				_rcv_pt_sel_max = 0;

				return true;
			}

			/**
			 * Initialize receive window for portal capability selectors
			 *
			 * \param utcb  UTCB of designated receiver thread
			 *
			 * Depending on the 'rcv_dirty' state of the message buffer, this
			 * function allocates a fresh receive window and clears 'rcv_dirty'.
			 */
			void rcv_prepare_pt_sel_window(Nova::Utcb *utcb)
			{
				if (rcv_dirty()) {
					_rcv_pt_base = cap_selector_allocator()->alloc(MAX_CAP_ARGS_LOG2);
					_rcv_dirty = false;
				}

				/* register receive window at the UTCB */
				utcb->crd_rcv = Nova::Obj_crd(rcv_pt_base(),MAX_CAP_ARGS_LOG2);
				utcb->crd_xlt = Nova::Obj_crd(0, sizeof(addr_t) * 8 - 12);
			}

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
			void post_ipc(Nova::Utcb *utcb) {
				_rcv_items = (utcb->items >> 16) & 0xffffu;
				_rcv_pt_sel_max = 0;
				_rcv_pt_sel_cnt = 0;

				for (unsigned i=0; i < _rcv_items; i++) {
					Nova::Utcb::Item * item = utcb->get_item(i);
					if (!item) break;
					if (_rcv_pt_sel_max >= MAX_CAP_ARGS) break;

					_rcv_pt_sel[_rcv_pt_sel_max].sel   = item->crd >> 12;
					_rcv_pt_sel[_rcv_pt_sel_max++].del = item->is_del();
				}
			}

	};


	template <unsigned BUF_SIZE>
	class Msgbuf : public Msgbuf_base
	{
		public:

			char buf[BUF_SIZE];

			Msgbuf() { _size = BUF_SIZE; }
	};
}

#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
