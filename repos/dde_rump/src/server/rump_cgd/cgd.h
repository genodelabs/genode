/**
 * \brief  Rump cgd header
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CGD_H_
#define _CGD_H_

#include <base/signal.h>
#include <os/server.h>

namespace Cgd {

	typedef Genode::uint64_t seek_off_t;

	class Params;

	class Device
	{
		private:

			int              _fd;         /* fd for cgd (raw) device */
			Genode::size_t   _blk_sz;     /* block size of cgd device */
			Genode::uint64_t _blk_cnt;    /* block count of cgd device */


		public:

			Device(int fd);

			~Device();

			char const *name() const;

			Genode::size_t block_size() const { return _blk_sz; }

			Genode::uint64_t block_count() const { return _blk_cnt; }

			Genode::size_t read(char *dst, Genode::size_t len, seek_off_t seek_offset);

			Genode::size_t write(char const *src, Genode::size_t len, seek_off_t seek_offset);

			static Device *configure(Genode::Allocator *alloc, Params const *p, char const *dev);
	};


	Device *init(Genode::Allocator *alloc, Server::Entrypoint &ep);
	void    deinit(Genode::Allocator *alloc, Device *dev);
}

#endif /* _CGD_H_ */
