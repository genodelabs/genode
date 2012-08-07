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

			enum {
				MAX_CAP_ARGS_LOG2 = 2,
				MAX_CAP_ARGS = 1 << MAX_CAP_ARGS_LOG2
			};

		protected:

			size_t _size;

			/**
			 * Number of portal-capability selectors to send
			 */
			size_t _snd_pt_sel_cnt;

			/**
			 * Portal capability selectors to delegate
			 */
			struct {
				addr_t   sel;
				unsigned rights;
				bool	 trans_map;
			} _snd_pt_sel [MAX_CAP_ARGS];

			/**
			 * Base of portal receive window
			 */
			addr_t _rcv_pt_base;

			struct {
				addr_t sel;
				bool   del;
			} _rcv_pt_sel [MAX_CAP_ARGS];

			/**
			 * Read counter for unmarshalling portal capability
			 * selectors
			 */
			unsigned _rcv_pt_sel_cnt;
			unsigned _rcv_pt_sel_max;

			/**
			 * Number of capabilities which has been received,
			 * reported by the kernel.
			 */
			unsigned _rcv_items;

			char _msg_start[];  /* symbol marks start of message */

		public:
			enum { INVALID_INDEX = ~0UL };

			/**
			 * Constructor
			 */
			Msgbuf_base()
			: _rcv_pt_base(INVALID_INDEX), _rcv_items(0)
			{
				rcv_reset();
				snd_reset();
			}

			~Msgbuf_base()
			{
				rcv_reset();
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
			inline bool snd_append_pt_sel(addr_t pt_sel,
			                              unsigned rights,
			                              bool trans_map)
			{
				if (_snd_pt_sel_cnt >= MAX_CAP_ARGS - 1)
					return false;

				_snd_pt_sel[_snd_pt_sel_cnt  ].sel       = pt_sel;
				_snd_pt_sel[_snd_pt_sel_cnt  ].rights    = rights;
				_snd_pt_sel[_snd_pt_sel_cnt++].trans_map = trans_map;
				return true;
			}

			/**
			 * Return number of marshalled portal-capability
			 * selectors
			 */
			inline size_t snd_pt_sel_cnt()
			{
				return _snd_pt_sel_cnt;
			}

			/**
			 * Return portal capability selector
			 *
			 * \param i  index (0 ... 'pt_sel_cnt()' - 1)
			 * \return   portal-capability range descriptor
			 *
			 * The returned object could be a null cap. Use
			 * is_null method to check for it.
			 */
			Nova::Obj_crd snd_pt_sel(addr_t i, bool &trans_map)
			{
				if (i >= _snd_pt_sel_cnt)
					return Nova::Obj_crd();

				trans_map = _snd_pt_sel[i].trans_map;
				return Nova::Obj_crd(_snd_pt_sel[i].sel, 0,
				                     _snd_pt_sel[i].rights);
			}

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
				_rcv_pt_base = INVALID_INDEX;
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
					return INVALID_INDEX;
			}

			/**
			 * Return true if receive window must be re-initialized
			 */
			bool rcv_invalid()
			{
				return _rcv_pt_base == INVALID_INDEX;
			}

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
			 * \result 'true'  -  receive window was freed
			 *         'false' -  portal selectors has been kept
			 */
			bool rcv_cleanup(bool keep)
			{
				/*
				 * If nothing has been used, revoke and free
				 * at once.
				 */
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
			void rcv_prepare_pt_sel_window(Nova::Utcb *utcb,
			                               addr_t rcv_window = INVALID_INDEX)
			{
				/*
				 * If a rcv_window was specified use solely
				 * the selector specified by rcv_window.
				 */
				if (rcv_window != INVALID_INDEX) {
					/*
					 * Cleanup if this msgbuf was already
					 * used
					 */
					if (!rcv_invalid()) rcv_cleanup(false);

					_rcv_pt_base = rcv_window;
				} else {
					if (rcv_invalid() || rcv_cleanup(true))
						_rcv_pt_base = cap_selector_allocator()->alloc(MAX_CAP_ARGS_LOG2);
				}

				addr_t max = 0;
				if (rcv_window == INVALID_INDEX)
					max = MAX_CAP_ARGS_LOG2;

				using namespace Nova;
				/* register receive window at the UTCB */
				utcb->crd_rcv = Obj_crd(rcv_pt_base(), max);
				/* Open maximal translate window */
				utcb->crd_xlt = Obj_crd(0, ~0UL);
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
			void post_ipc(Nova::Utcb *utcb)
			{
				using namespace Nova;

				_rcv_items = (utcb->items >> 16) & 0xffffu;
				_rcv_pt_sel_max = 0;
				_rcv_pt_sel_cnt = 0;

				addr_t max = 1UL << utcb->crd_rcv.order();
				for (unsigned i = 0; i < _rcv_items; i++) {
					Utcb::Item * item = utcb->get_item(i);
					if (!item ||
					    _rcv_pt_sel_max >= max) break;

					Crd cap = Crd(item->crd);
					_rcv_pt_sel[_rcv_pt_sel_max].sel   = cap.is_null() ? INVALID_INDEX : cap.base();
					_rcv_pt_sel[_rcv_pt_sel_max++].del = item->is_del();
				}
				/*
				 * If a specific rcv_window has been specified,
				 * (see rcv_prepare_pt_sel_window) then the
				 * caller want to take care about freeing the
				 * selector. Make the _rcv_pt_base invalid so
				 * that it is not cleanup twice.
				 */
				if (max != MAX_CAP_ARGS)
					_rcv_pt_base = INVALID_INDEX;
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
