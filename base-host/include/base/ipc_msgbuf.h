/*
 * \brief  Dummy IPC message buffer
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

namespace Genode {

	class Msgbuf_base
	{
		private:

			size_t _size;

		public:

			char buf[];

			/**
			 * Return size of message buffer
			 */
			inline size_t size() const { return _size; };
	};

	template <unsigned BUF_SIZE>
	class Msgbuf : public Msgbuf_base { };
}

#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
