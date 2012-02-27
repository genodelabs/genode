/*
 * \brief  IPC message buffer layout for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2010-11-30
 *
 * On Fiasco.OC, IPC is used to transmit plain data and capabilities.
 * Therefore the message buffer contains both categories of payload.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

/* Genode includes */
#include <base/cap_sel_alloc.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
}

namespace Genode {

	class Msgbuf_base
	{
		public:

			enum { MAX_CAP_ARGS_LOG2 = 2, MAX_CAP_ARGS = 1 << MAX_CAP_ARGS_LOG2 };

		protected:

			size_t _size;

			/**
			 * Number of capability selectors to send.
			 */
			size_t _snd_cap_sel_cnt;

			/**
			 * Capability selectors to delegate.
			 */
			addr_t _snd_cap_sel[MAX_CAP_ARGS];

			/**
			 * Base of capability receive window.
			 */
			addr_t _rcv_cap_sel_base;

			/**
			 * Read counter for unmarshalling portal capability selectors
			 */
			addr_t _rcv_cap_sel_cnt;

			char _msg_start[];  /* symbol marks start of message */

		public:

			/**
			 * Constructor
			 */
			Msgbuf_base()
			: _rcv_cap_sel_base(cap_alloc()->alloc(MAX_CAP_ARGS))
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
			inline size_t size() const { return _size; };

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &_msg_start[0]; };

			/**
			 * Reset portal capability selector payload
			 */
			inline void snd_reset() { _snd_cap_sel_cnt = 0; }

			/**
			 * Append capability selector to message buffer
			 */
			inline bool snd_append_cap_sel(addr_t cap_sel)
			{
				if (_snd_cap_sel_cnt >= MAX_CAP_ARGS)
					return false;

				_snd_cap_sel[_snd_cap_sel_cnt++] = cap_sel;
				return true;
			}

			/**
			 * Return number of marshalled capability selectors
			 */
			inline size_t snd_cap_sel_cnt() { return _snd_cap_sel_cnt; }

			/**
			 * Return capability selector to send.
			 *
			 * \param i  index (0 ... 'snd_cap_sel_cnt()' - 1)
			 * \return   capability selector, or 0 if index is invalid
			 */
			addr_t snd_cap_sel(unsigned i) {
				return i < _snd_cap_sel_cnt ? _snd_cap_sel[i] : 0; }

			/**
			 * Return address of capability receive window.
			 */
			addr_t rcv_cap_sel_base() { return _rcv_cap_sel_base; }

			/**
			 * Reset capability receive window
			 */
			void rcv_reset() { _rcv_cap_sel_cnt = 0; }

			/**
			 * Return next received capability selector.
			 *
			 * \return   capability selector, or 0 if index is invalid
			 */
			addr_t rcv_cap_sel() {
				return _rcv_cap_sel_base + _rcv_cap_sel_cnt++ * Fiasco::L4_CAP_SIZE; }
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
