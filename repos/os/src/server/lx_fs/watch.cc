/*
 * \brief  File-system node
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* local includes */
#include "watch.h"
#include "notifier.h"


unsigned long Lx_fs::Watch_node::_inode(char const *path)
{
	struct stat s   { };
	int         ret { lstat(path, &s) };
	if (ret == -1)
		throw Lookup_failed();

	return s.st_ino;
}


Lx_fs::Watch_node::Watch_node(Env              &env,
                              char const       *path,
                              Response_handler &response_handler,
                              Notifier          &notifier)
:
	Node { _inode(path) },
	_env { env },
	_response_handler { response_handler },
	_notifier { notifier }
{
	name(path);

	if (notifier.add_watch(path, *this) < 0) {
		throw Lookup_failed { };
	}
}

Lx_fs::Watch_node::~Watch_node()
{
	_notifier.remove_watch(name(), *this);
}


void Lx_fs::Watch_node::_handle_notify()
{
	mark_as_updated();
	_acked_packet = Packet_descriptor { Packet_descriptor { },
	                                    Node_handle { _open_node->id().value },
	                                    Packet_descriptor::CONTENT_CHANGED,
	                                    0, 0 };
	_acked_packet.succeeded(true);

	_response_handler.handle_watch_node_response(*this);
}
