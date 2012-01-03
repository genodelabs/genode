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
			int _snd_pt_sel[MAX_CAP_ARGS];

			/**
			 * Base of portal receive window
			 */
			int _rcv_pt_base;

			/**
			 * Read counter for unmarshalling portal capability selectors
			 */
			int _rcv_pt_sel_cnt;

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
			inline bool snd_append_pt_sel(int pt_sel)
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
			int snd_pt_sel(unsigned i) { return i < _snd_pt_sel_cnt ? _snd_pt_sel[i] : -1; }

			/**
			 * Request current portal-receive window
			 */
			int rcv_pt_base() { return _rcv_pt_base; }

			/**
			 * Reset portal-capability receive window
			 */
			void rcv_reset() { _rcv_pt_sel_cnt = 0; }

			/**
			 * Return received portal-capability selector
			 */
			int rcv_pt_sel()
			{
				_rcv_dirty = true;
				return _rcv_pt_base + _rcv_pt_sel_cnt++;
			}

			/**
			 * Return true if receive window must be re-initialized
			 *
			 * After reading portal selectors from the message buffer using
			 * 'rcv_pt_sel()', we assume that the IDC call populared the
			 * current receive window with one or more portal capabilities.
			 * To enable the reception of portal capability selectors for the
			 * next IDC, we need a fresh receive window.
			 */
			bool rcv_dirty() { return _rcv_dirty; }

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
