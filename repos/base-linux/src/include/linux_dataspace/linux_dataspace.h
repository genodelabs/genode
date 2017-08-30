/*
 * \brief  Linux-specific dataspace interface
 * \author Norman Feske
 * \date   2006-07-05
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LINUX_DATASPACE__LINUX_DATASPACE_H_
#define _INCLUDE__LINUX_DATASPACE__LINUX_DATASPACE_H_

#include <dataspace/dataspace.h>
#include <base/stdint.h>
#include <base/ipc.h>
#include <base/rpc.h>

namespace Genode {

	struct Linux_dataspace : Dataspace
	{
		enum { FNAME_LEN = 64 };
		struct Filename { char buf[FNAME_LEN]; };

		virtual ~Linux_dataspace() { }

		/**
		 * Request name of file that represents the dataspace on Linux
		 *
		 * This function is used for calling execve on files passed as ROM
		 * dataspaces.
		 */
		virtual Filename fname() = 0;

		/**
		 * Request file descriptor of the dataspace
		 */
		virtual Untyped_capability fd() = 0;

		/*********************
		 ** RPC declaration **
		 *********************/


		GENODE_RPC(Rpc_fname, Filename, fname);
		GENODE_RPC(Rpc_fd, Untyped_capability, fd);
		GENODE_RPC_INTERFACE_INHERIT(Dataspace, Rpc_fname, Rpc_fd);
	};
}

#endif /* _INCLUDE__LINUX_DATASPACE__LINUX_DATASPACE_H_ */
