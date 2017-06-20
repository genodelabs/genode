/*
 * \brief  File-system listener
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_SYSTEM__LISTENER_H_
#define _FILE_SYSTEM__LISTENER_H_

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

namespace File_system {

	typedef File_system::Session_rpc_object::Tx::Sink Sink;

	class Listener : public Genode::List<Listener>::Element
	{

		private:

			Genode::Lock  _lock;
			Sink         &_sink;
			Node_handle   _handle;
			bool          _marked_as_updated;

		public:

			Listener(Sink &sink, Node_handle handle)
			: _sink(sink), _handle(handle), _marked_as_updated(false) { }

			void notify()
			{
				Genode::Lock::Guard guard(_lock);

				if (_marked_as_updated && _sink.ready_to_ack()) {
					_sink.acknowledge_packet(Packet_descriptor(
						_handle, Packet_descriptor::CONTENT_CHANGED));
					_marked_as_updated = false;
				}
			}

			void mark_as_updated()
			{
				Genode::Lock::Guard guard(_lock);

				_marked_as_updated = true;
			}
	};

}

#endif /* _FILE_SYSTEM__LISTENER_H_ */
