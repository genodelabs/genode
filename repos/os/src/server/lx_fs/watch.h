/*
 * \brief  File-system node for watched files/directories
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

#ifndef _WATCH_H_
#define _WATCH_H_

/* Genode includes */
#include <base/id_space.h>
#include <file_system/node.h>
#include <file_system/open_node.h>
#include <os/path.h>

/* local includes */
#include "node.h"
#include "open_node.h"


namespace Lx_fs {

	using namespace File_system;

	/* forward declaration */
	class Notifier;

	class Watch_node;
}


class Lx_fs::Watch_node final : public Lx_fs::Node
{
	public:

		using Packet_descriptor = File_system::Packet_descriptor;
		using Fs_open_node      = File_system::Open_node<Lx_fs::Watch_node>;

		struct Response_handler : Genode::Interface
		{
			virtual void handle_watch_node_response(Watch_node &) = 0;
		};

	private:

		using Signal_handler = Genode::Signal_handler<Watch_node>;

		/*
		 * Noncopyable
		 */
		Watch_node(Watch_node const &) = delete;
		Watch_node &operator = (Watch_node const &) = delete;

		Genode::Env        &_env;
		Response_handler   &_response_handler;
		Notifier           &_notifier;
		Signal_handler      _notify_handler { _env.ep(), *this, &Watch_node::_handle_notify };
		Packet_descriptor   _acked_packet { };
		Fs_open_node       *_open_node { nullptr };

		void _handle_notify();

		unsigned long _inode(char const *path);

	public:

		Watch_node(Genode::Env      &env,
		           char const       *path,
		           Response_handler &response_handler,
		           Notifier         &notifier);

		~Watch_node();

		Signal_handler &notify_handler() { return _notify_handler; }

		void update_modification_time(Timestamp const) override { }
		size_t read(char *, size_t, seek_off_t) override { return 0; }
		size_t write(char const *, size_t, seek_off_t) override { return 0; }
		Status status() override { return Status { }; }

		Packet_descriptor &acked_packet() { return _acked_packet; }

		void open_node(Fs_open_node *open_node) { _open_node = open_node; }
		Fs_open_node *open_node() { return _open_node; }
};

#endif /* _WATCH_H_ */
