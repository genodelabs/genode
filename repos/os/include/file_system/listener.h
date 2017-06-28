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
		public:

			struct Version { unsigned value; };

		private:

			Genode::Lock      _lock;
			Sink             &_sink;
			Node_handle const _handle;

			/*
			 * Version at the time when the file was opened
			 */
			Version _handed_out_version;

			/*
			 * Version at the time when we issued the most recent notification
			 */
			Version _notified_version = _handed_out_version;

		public:

			Listener(Sink &sink, Node_handle handle, Version handed_out_version)
			: _sink(sink), _handle(handle), _handed_out_version(handed_out_version)
			{ }

			/*
			 * Called on close of written files, on sync, or on arrival
			 * of a client's CONTENT_CHANGED packet.
			 */
			void notify(Version curr_version)
			{
				Genode::Lock::Guard guard(_lock);

				if (curr_version.value == _handed_out_version.value)
					return;

				if (curr_version.value == _notified_version.value)
					return;

				if (_sink.ready_to_ack())
					_sink.acknowledge_packet(Packet_descriptor(
						_handle, Packet_descriptor::CONTENT_CHANGED));

				_notified_version = curr_version;
			}

			/*
			 * Called during read
			 */
			void handed_out_version(Version version)
			{
				Genode::Lock::Guard guard(_lock);

				_handed_out_version = version;
			}
	};

}

#endif /* _FILE_SYSTEM__LISTENER_H_ */
