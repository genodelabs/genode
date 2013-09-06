/*
 * \brief  IPC message buffer layout for NOVA
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-10-02
 *
 * On NOVA, we use IPC to transmit plain data and for capability delegation
 * and capability translation.
 * Therefore the message buffer contains three categories of payload. The
 * capability-specific part are the members '_snd_pt*' (sending capability
 * selectors) and '_rcv_pt*' (receiving capability selectors).
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
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
#include <nova/util.h>

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
				bool     trans_map;
			} _snd_pt_sel [MAX_CAP_ARGS];

			/**
			 * Base of portal receive window
			 */
			addr_t _rcv_pt_base;

			struct {
				addr_t sel;
				bool   del;
			} _rcv_pt_sel [MAX_CAP_ARGS];

			enum { FREE_INVALID, FREE_SEL, UNUSED_CAP, USED_CAP }
				_rcv_pt_cap_free [MAX_CAP_ARGS];

			/**
			 * Read counter for unmarshalling portal capability
			 * selectors
			 */
			unsigned short _rcv_pt_sel_cnt;
			unsigned short _rcv_pt_sel_max;
			unsigned short _rcv_wnd_log2;

			char _msg_start[];  /* symbol marks start of message */

		public:

			enum { INVALID_INDEX = ~0UL };

			/**
			 * Constructor
			 */
			Msgbuf_base()
			: _rcv_pt_base(INVALID_INDEX), _rcv_wnd_log2(MAX_CAP_ARGS_LOG2)
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
			inline bool snd_append_pt_sel(addr_t pt_sel, unsigned rights,
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
			inline size_t snd_pt_sel_cnt() const
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
			Nova::Obj_crd snd_pt_sel(addr_t i, bool &trans_map) const
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
			addr_t rcv_pt_base() const { return _rcv_pt_base; }

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
			bool rcv_invalid() const
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
			 * \result 'true'  -  receive window must be re-initialized
			 *         'false' -  portal selectors has been kept
			 */
			bool rcv_cleanup(bool keep, unsigned short const new_max = MAX_CAP_ARGS)
			{
				/* mark used mapped capabilities as used to prevent freeing */
				bool reinit = false;
				for (unsigned i = 0; i < _rcv_pt_sel_cnt; i++) {
					if (!_rcv_pt_sel[i].del)
						continue;

					/* should never happen */
					if (_rcv_pt_sel[i].sel < rcv_pt_base() ||
						(_rcv_pt_sel[i].sel >= rcv_pt_base() + MAX_CAP_ARGS))
							nova_die();

					_rcv_pt_cap_free [_rcv_pt_sel[i].sel - rcv_pt_base()] = USED_CAP;
					reinit = true;
				}

				/* revoke received caps which are unused */
				for (unsigned i = 0; i < MAX_CAP_ARGS; i++) {
					if (i < new_max && _rcv_pt_cap_free[i] == FREE_INVALID)
						reinit = true;

					if (_rcv_pt_cap_free[i] == UNUSED_CAP)
						Nova::revoke(Nova::Obj_crd(rcv_pt_base() + i, 0), true);
				}

				_rcv_pt_sel_cnt = 0;
				_rcv_pt_sel_max = 0;

				/* we can keep the cap selectors if none was used */
				if (keep && !reinit) {
					/* free rest of indexes if new_max is smaller then last window */
					for (unsigned i = new_max; i < MAX_CAP_ARGS; i++)
						if (_rcv_pt_cap_free[i] == FREE_SEL)
							cap_selector_allocator()->free(rcv_pt_base() + i, 0);

					return false;
				}

				/* keep used selectors, free up rest */	
				for (unsigned i = 0; i < MAX_CAP_ARGS; i++) {
					if (_rcv_pt_cap_free[i] == UNUSED_CAP ||
						_rcv_pt_cap_free[i] == FREE_SEL)
							cap_selector_allocator()->free(rcv_pt_base() + i, 0);
				}

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
			bool prepare_rcv_window(Nova::Utcb *utcb,
			                        addr_t rcv_window = INVALID_INDEX)
			{
				/* open maximal translate window */
				utcb->crd_xlt = Nova::Obj_crd(0, ~0UL);

				/* use receive window if specified */
				if (rcv_window != INVALID_INDEX) {
					/* cleanup if receive window already used */
					if (!rcv_invalid()) rcv_cleanup(false);

					_rcv_pt_base = rcv_window;

					/* open receive window */
					utcb->crd_rcv = Nova::Obj_crd(rcv_pt_base(), _rcv_wnd_log2);
					return true;
				}

				/* allocate receive window if necessary, otherwise use old one */
				if (rcv_invalid() || rcv_cleanup(true, 1U << _rcv_wnd_log2))
				{
					try {
						_rcv_pt_base = cap_selector_allocator()->alloc(_rcv_wnd_log2);
					} catch (Bit_array_out_of_indexes) {
						_rcv_pt_base = INVALID_INDEX;
						/* no mappings can be received */
						utcb->crd_rcv = Nova::Obj_crd();
						return false;
					}
				}

				/* open receive window */
				utcb->crd_rcv = Nova::Obj_crd(rcv_pt_base(), _rcv_wnd_log2);
				return true;
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
			void post_ipc(Nova::Utcb *utcb, addr_t const rcv_window = INVALID_INDEX)
			{
				using namespace Nova;

				unsigned const rcv_items = (utcb->items >> 16) & 0xffffu;

				_rcv_pt_sel_max = 0;
				_rcv_pt_sel_cnt = 0;

				unsigned short const max = 1U << utcb->crd_rcv.order();
				if (max > MAX_CAP_ARGS)
					nova_die();

				for (unsigned short i = 0; i < MAX_CAP_ARGS; i++) 
					_rcv_pt_cap_free [i] = (i >= max) ? FREE_INVALID : FREE_SEL;

				addr_t max = 1UL << utcb->crd_rcv.order();

				for (unsigned i = 0; i < rcv_items; i++) {
					Utcb::Item * item = utcb->get_item(i);
					if (!item)
						break;

					Crd cap = Crd(item->crd);

					/* track which items we got mapped */
					if (!cap.is_null() && item->is_del()) {
						/* should never happen */
						if (cap.base() < rcv_pt_base() ||
						    (cap.base() >= rcv_pt_base() + max))
							nova_die();
						_rcv_pt_cap_free [cap.base() - rcv_pt_base()] = UNUSED_CAP;
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
